/*
 * EPI — firmware pre-prototypu na Seeed XIAO nRF54L15 (Sense).
 *
 * Warstwa sprzętowa. Cała logika rozstrzygająca (automat detekcji, dioda,
 * bateria, przycisk) siedzi w ../lib i jest testowana na komputerze —
 * tutaj zostaje odczyt czujników, PWM, wyjścia alarmu i BLE.
 *
 * Stan pliku: szkielet integracji. Kompilacja wymaga nRF Connect SDK i
 * uzupełnienia pinctrl w boards/xiao_nrf54l15_cpuapp.overlay.
 */

#include <math.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/sys/reboot.h>

#include "ble_epi.h"
#include "epi_battery.h"
#include "epi_button.h"
#include "epi_detector.h"
#include "epi_led.h"

LOG_MODULE_REGISTER(epi, CONFIG_LOG_DEFAULT_LEVEL);

/* Okresy pracy. Detektor chodzi w tym samym rytmie co w aplikacji. */
#define TICK_LED_MS       10
#define TICK_DETECTOR_MS  250
#define TICK_BATTERY_MS   30000
#define IMU_HZ            52
#define SOS_WINDOW_MS     10000  /* czas na anulowanie SOS drugim kliknięciem */

static const struct pwm_dt_spec led_r = PWM_DT_SPEC_GET(DT_NODELABEL(led_r));
static const struct pwm_dt_spec led_g = PWM_DT_SPEC_GET(DT_NODELABEL(led_g));
static const struct pwm_dt_spec led_b = PWM_DT_SPEC_GET(DT_NODELABEL(led_b));

static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(DT_ALIAS(epi_button), gpios);
static const struct gpio_dt_spec buzzer =
    GPIO_DT_SPEC_GET(DT_ALIAS(epi_buzzer), gpios);
static const struct gpio_dt_spec motor =
    GPIO_DT_SPEC_GET(DT_ALIAS(epi_motor), gpios);

/* Alias wskazuje węzeł IMU z pliku płytki; nazwa sterownika dla
 * LSM6DS3TR-C zależy od wersji SDK, więc nie jest tu wpisana na sztywno. */
static const struct device *imu = DEVICE_DT_GET(DT_ALIAS(epi_imu));

static struct {
    epi_detector det;
    epi_button   btn;
    epi_batt     batt;
    epi_led_input led;
    uint32_t boot_ms;
    uint32_t sos_deadline_ms;
    bool     alarm_output;
} app;

static uint32_t now_ms(void)
{
    return (uint32_t)k_uptime_get_32();
}

/* --- dioda ------------------------------------------------------------- */

static void led_write(epi_rgb c)
{
    /* Wspólna anoda: wypełnienie 0 to pełna jasność, dlatego wartość jest
     * odwracana. Przy wspólnej katodzie wystarczy usunąć odejmowanie. */
    (void)pwm_set_pulse_dt(&led_r, (led_r.period * (255u - c.r)) / 255u);
    (void)pwm_set_pulse_dt(&led_g, (led_g.period * (255u - c.g)) / 255u);
    (void)pwm_set_pulse_dt(&led_b, (led_b.period * (255u - c.b)) / 255u);
}

static void led_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(led_work, led_work_fn);

static void led_work_fn(struct k_work *work)
{
    uint32_t t = now_ms();

    ARG_UNUSED(work);
    app.led.uptime_ms = t - app.boot_ms;
    app.led.detector = app.det.state;
    led_write(epi_led_render(&app.led, t));
    k_work_reschedule(&led_work,
                      K_MSEC(epi_led_tick_ms(epi_led_mode_for(&app.led))));
}

/* --- alarm lokalny ----------------------------------------------------- */

/* Buzzer i silnik pracują impulsami: ciągła praca to około 120 mA, czyli
 * kilkanaście procent pojemności ogniwa na godzinę. */
static void alarm_output_set(bool on)
{
    if (app.alarm_output == on) {
        return;
    }
    app.alarm_output = on;
    (void)gpio_pin_set_dt(&buzzer, on ? 1 : 0);
    (void)gpio_pin_set_dt(&motor, on ? 1 : 0);
}

static void alarm_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(alarm_work, alarm_work_fn);

static void alarm_work_fn(struct k_work *work)
{
    bool active = (app.det.state == EPI_DET_ALARM) || app.led.sos_active;

    ARG_UNUSED(work);
    if (!active) {
        alarm_output_set(false);
        return;
    }
    alarm_output_set(!app.alarm_output);
    k_work_reschedule(&alarm_work, K_MSEC(app.alarm_output ? 400 : 600));
}

/* Krótkie potwierdzenie gestu: sam silnik, bez dźwięku. */
static void haptic_blip(uint16_t ms)
{
    (void)gpio_pin_set_dt(&motor, 1);
    k_sleep(K_MSEC(ms));
    (void)gpio_pin_set_dt(&motor, 0);
}

/* --- przycisk ---------------------------------------------------------- */

static void power_off_device(void)
{
    alarm_output_set(false);
    led_write((epi_rgb){0, 0, 0});
    app.led.powered_on = false;
    app.det.state = EPI_DET_OFF;
    haptic_blip(300);

    /* Wybudzenie przyciskiem z trybu System OFF. */
    (void)gpio_pin_interrupt_configure_dt(&button, GPIO_INT_LEVEL_ACTIVE);
    sys_poweroff();
}

static void on_button_event(epi_btn_event evt, uint32_t t)
{
    switch (evt) {
    case EPI_BTN_EVT_CLICK:
        if (app.led.sos_active || app.det.state == EPI_DET_ALARM ||
            app.det.state == EPI_DET_CONFIRMING) {
            /* Drugie kliknięcie kasuje alarm albo przerywa okno potwierdzenia. */
            app.led.sos_active = false;
            epi_det_cancel(&app.det, t);
            alarm_output_set(false);
            haptic_blip(80);
        } else {
            app.led.sos_active = true;
            app.sos_deadline_ms = t + SOS_WINDOW_MS;
            haptic_blip(150);
            k_work_reschedule(&alarm_work, K_NO_WAIT);
        }
        ble_epi_notify_state(app.det.state, &app.det.an);
        break;
    case EPI_BTN_EVT_LONG:
        haptic_blip(200);
        power_off_device();
        break;
    case EPI_BTN_EVT_RESET:
        sys_reboot(SYS_REBOOT_COLD);
        break;
    default:
        break;
    }
}

static void button_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(button_work, button_work_fn);

static void button_work_fn(struct k_work *work)
{
    uint32_t t = now_ms();
    bool pressed = gpio_pin_get_dt(&button) > 0;
    epi_btn_event evt = epi_button_update(&app.btn, pressed, t);

    ARG_UNUSED(work);
    if (evt != EPI_BTN_EVT_NONE) {
        on_button_event(evt, t);
    }
    /* Odpytywanie tylko dopóki przycisk jest w ruchu; poza tym budzi przerwanie. */
    if (pressed || app.btn.stable) {
        k_work_reschedule(&button_work, K_MSEC(10));
    }
}

static struct gpio_callback button_cb;

static void button_isr(const struct device *dev, struct gpio_callback *cb,
                       uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    k_work_reschedule(&button_work, K_NO_WAIT);
}

/* --- czujniki ---------------------------------------------------------- */

static void imu_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(imu_work, imu_work_fn);

static void imu_work_fn(struct k_work *work)
{
    struct sensor_value acc[3];
    float x, y, z;

    ARG_UNUSED(work);
    if (sensor_sample_fetch(imu) == 0 &&
        sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, acc) == 0) {
        x = (float)sensor_value_to_double(&acc[0]);
        y = (float)sensor_value_to_double(&acc[1]);
        z = (float)sensor_value_to_double(&acc[2]);
        /* Ten sam sygnał, który analizuje aplikacja: moduł wektora razem
         * z grawitacją; składową stałą usuwa dopiero okno analizy. */
        epi_det_push_motion(&app.det, now_ms(), sqrtf(x * x + y * y + z * z));
    }
    k_work_reschedule(&imu_work, K_MSEC(1000 / IMU_HZ));
}

/* Pulsoksymetr jest największym odbiornikiem w budżecie energii, więc
 * w stanie IDLE pracuje z małym wypełnieniem, a pełne próbkowanie włącza
 * się razem ze stanem SUSPECT. */
static void ppg_set_active(bool full_rate)
{
    ARG_UNUSED(full_rate);
    /* TODO: sterowanie modułem MAX30102 przez I2C — zależnie od tego, czy
     * używamy surowych próbek, czy wyniku algorytmu z modułu DFRobot. */
}

static void detector_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(detector_work, detector_work_fn);

static void detector_work_fn(struct k_work *work)
{
    uint32_t t = now_ms();
    epi_det_state before = app.det.state;
    epi_det_state after;

    ARG_UNUSED(work);
    after = epi_det_tick(&app.det, t);

    if (after != before) {
        ppg_set_active(after != EPI_DET_IDLE);
        ble_epi_notify_state(after, &app.det.an);
        if (after == EPI_DET_ALARM || after == EPI_DET_CONFIRMING) {
            k_work_reschedule(&alarm_work, K_NO_WAIT);
        }
    }
    if (app.led.sos_active && t >= app.sos_deadline_ms) {
        /* Okno anulowania minęło: powiadomienie idzie do aplikacji. */
        ble_epi_notify_state(EPI_DET_ALARM, &app.det.an);
    }
    k_work_reschedule(&detector_work, K_MSEC(TICK_DETECTOR_MS));
}

/* --- bateria ----------------------------------------------------------- */

static void battery_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(battery_work, battery_work_fn);

static void battery_work_fn(struct k_work *work)
{
    uint16_t mv;

    ARG_UNUSED(work);
    /* TODO: odczyt z ADC przez dzielnik na płytce; kanał i odniesienie
     * z overlaya, przeliczenie przez epi_batt_mv_from_adc().
     * TODO: app.led.charger_present i charge_complete z wyprowadzeń D10 i D9. */
    mv = app.batt.primed ? app.batt.mv_filtered : EPI_BATT_FULL_MV;

    app.led.soc = epi_batt_update(&app.batt, mv, app.led.charger_present);
    ble_epi_notify_battery(app.led.soc);

    if (!app.led.charger_present && mv < EPI_BATT_CUTOFF_MV) {
        LOG_WRN("napięcie ogniwa %u mV, wyłączenie", mv);
        power_off_device();
    }
    k_work_reschedule(&battery_work, K_MSEC(TICK_BATTERY_MS));
}

/* --- start ------------------------------------------------------------- */

int main(void)
{
    epi_det_cfg cfg;

    app.boot_ms = now_ms();
    app.led.powered_on = true;
    app.led.brightness = 60; /* jasność dobrana tak, żeby nie razić w nocy */

    epi_det_defaults(&cfg);
    epi_det_init(&app.det, &cfg, app.boot_ms);
    epi_button_init(&app.btn, app.boot_ms);
    epi_batt_init(&app.batt);
    app.led.soc = 100;

    if (!pwm_is_ready_dt(&led_r) || !device_is_ready(button.port) ||
        !device_is_ready(imu)) {
        LOG_ERR("brak gotowego urządzenia");
        app.led.fault = true;
    }

    (void)gpio_pin_configure_dt(&button, GPIO_INPUT);
    (void)gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&button_cb, button_isr, BIT(button.pin));
    (void)gpio_add_callback(button.port, &button_cb);

    (void)gpio_pin_configure_dt(&buzzer, GPIO_OUTPUT_INACTIVE);
    (void)gpio_pin_configure_dt(&motor, GPIO_OUTPUT_INACTIVE);

    (void)ble_epi_init();

    k_work_reschedule(&led_work, K_NO_WAIT);
    k_work_reschedule(&imu_work, K_NO_WAIT);
    k_work_reschedule(&detector_work, K_MSEC(TICK_DETECTOR_MS));
    k_work_reschedule(&battery_work, K_NO_WAIT);

    haptic_blip(120); /* potwierdzenie włączenia */
    return 0;
}
