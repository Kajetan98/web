/* Testy logiki niezależnej od sprzętu. Budowanie i uruchomienie: make test */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "epi_battery.h"
#include "epi_button.h"
#include "epi_detector.h"
#include "epi_led.h"

static int failures;
static int checks;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        checks++;                                                           \
        if (!(cond)) {                                                      \
            failures++;                                                     \
            printf("FAIL %s:%d  ", __FILE__, __LINE__);                     \
            printf(__VA_ARGS__);                                            \
            printf("\n");                                                   \
        }                                                                   \
    } while (0)

static epi_led_input base_input(void)
{
    epi_led_input in;
    memset(&in, 0, sizeof(in));
    in.powered_on = true;
    in.soc = 80;
    in.detector = EPI_DET_IDLE;
    in.uptime_ms = 60000;
    in.brightness = 255;
    return in;
}

static void test_led_priority(void)
{
    epi_led_input in = base_input();

    in.powered_on = false;
    CHECK(epi_led_mode_for(&in) == EPI_LED_OFF, "wyłączone i bez ładowarki");

    in.charger_present = true;
    CHECK(epi_led_mode_for(&in) == EPI_LED_CHARGING,
          "ładowanie widoczne także przy wyłączonym urządzeniu");
    in.charge_complete = true;
    CHECK(epi_led_mode_for(&in) == EPI_LED_CHARGED, "ogniwo naładowane");

    in.powered_on = true;
    in.detector = EPI_DET_ALARM;
    CHECK(epi_led_mode_for(&in) == EPI_LED_ALARM, "alarm wyprzedza ładowanie");

    in.detector = EPI_DET_CONFIRMING;
    in.charger_present = false;
    in.charge_complete = false;
    CHECK(epi_led_mode_for(&in) == EPI_LED_CONFIRMING, "okno potwierdzenia");

    in.detector = EPI_DET_IDLE;
    in.sos_active = true;
    CHECK(epi_led_mode_for(&in) == EPI_LED_ALARM, "SOS z przycisku");
    in.sos_active = false;

    in.uptime_ms = 100;
    CHECK(epi_led_mode_for(&in) == EPI_LED_BOOT, "pokaz baterii po włączeniu");
    in.uptime_ms = EPI_LED_BOOT_MS;
    CHECK(epi_led_mode_for(&in) == EPI_LED_MONITOR, "po pokazie praca normalna");

    in.soc = EPI_LED_SOC_RED;
    CHECK(epi_led_mode_for(&in) == EPI_LED_LOW_BATTERY, "próg niskiej baterii");
    in.soc = EPI_LED_SOC_RED + 1u;
    CHECK(epi_led_mode_for(&in) == EPI_LED_MONITOR, "tuż nad progiem");

    in.fault = true;
    CHECK(epi_led_mode_for(&in) == EPI_LED_FAULT, "awaria czujnika");
}

static void test_led_soc_color(void)
{
    epi_rgb full = epi_led_soc_color(100);
    epi_rgb half = epi_led_soc_color(55);
    epi_rgb low = epi_led_soc_color(EPI_LED_SOC_RED);
    epi_rgb empty = epi_led_soc_color(0);
    int prev_r = -1;
    int prev_g = 1000;
    uint8_t soc;

    CHECK(full.g > 0 && full.r == 0 && full.b == 0, "100 %% czysto zielony");
    CHECK(half.r > 0 && half.g > 0 && half.b == 0, "w połowie mieszanka bez niebieskiego");
    CHECK(low.r > 0 && low.g == 0, "przy progu czysto czerwony");
    CHECK(empty.r == low.r && empty.g == 0, "poniżej progu bez zmian");

    /* Czerwony rośnie, a zielony maleje w miarę rozładowania. */
    for (soc = 100; soc > EPI_LED_SOC_RED; soc--) {
        epi_rgb c = epi_led_soc_color(soc);
        CHECK(c.r >= prev_r || soc == 100, "czerwony nie maleje przy soc=%u", soc);
        CHECK(c.g <= prev_g, "zielony nie rośnie przy soc=%u", soc);
        CHECK(c.b == 0, "niebieski wyłączony przy soc=%u", soc);
        prev_r = c.r;
        prev_g = c.g;
    }
}

static void test_led_patterns(void)
{
    epi_led_input in = base_input();
    epi_rgb c;
    uint32_t t;
    uint32_t lit = 0;

    /* Praca normalna: 30 ms błysku na 5 s. */
    for (t = 0; t < 5000; t += 5) {
        c = epi_led_render(&in, t);
        if (c.r || c.g || c.b) {
            lit += 5;
        }
    }
    CHECK(lit == 30, "wypełnienie w trybie MONITOR = %u ms", lit);

    /* Alarm: 4 Hz, wypełnienie 50 %. */
    in.detector = EPI_DET_ALARM;
    lit = 0;
    for (t = 0; t < 1000; t += 5) {
        c = epi_led_render(&in, t);
        if (c.r || c.g || c.b) {
            lit += 5;
        }
    }
    CHECK(lit == 500, "wypełnienie alarmu = %u ms na sekundę", lit);
    c = epi_led_render(&in, 0);
    CHECK(c.r > 0 && c.g == 0 && c.b == 0, "alarm świeci czerwono");

    /* Alarm ignoruje ustawienie jasności. */
    in.brightness = 10;
    c = epi_led_render(&in, 0);
    CHECK(c.r == 255, "alarm z pełną jasnością, r=%u", c.r);
    in.brightness = 255;

    /* Niska bateria: dwa błyski w okresie. */
    in.detector = EPI_DET_IDLE;
    in.soc = 5;
    {
        int edges = 0;
        int prev = 0;
        for (t = 0; t < 5000; t += 5) {
            c = epi_led_render(&in, t);
            int on = (c.r || c.g || c.b) ? 1 : 0;
            if (on && !prev) {
                edges++;
            }
            prev = on;
        }
        CHECK(edges == 2, "liczba błysków przy niskiej baterii = %d", edges);
    }

    /* Ładowanie: obwiednia oddechu przechodzi przez zero i maksimum. */
    in.soc = 40;
    in.charger_present = true;
    c = epi_led_render(&in, 0);
    CHECK(c.b == 0 && c.r == 0, "oddech zaczyna się od zgaszonej diody");
    c = epi_led_render(&in, 1500);
    CHECK(c.b > 0, "w połowie okresu dioda świeci na niebiesko, b=%u", c.b);

    /* Naładowane: świecenie ciągłe na zielono. */
    in.charge_complete = true;
    c = epi_led_render(&in, 1234);
    CHECK(c.g > 0 && c.r == 0, "po naładowaniu zielony ciągły");
}

static void test_battery(void)
{
    epi_batt b;
    uint8_t soc;
    uint16_t mv;
    int prev = -1;

    CHECK(epi_batt_soc_from_mv(4300) == 100, "powyżej pełnego");
    CHECK(epi_batt_soc_from_mv(4200) == 100, "pełne");
    CHECK(epi_batt_soc_from_mv(3300) == 0, "puste");
    CHECK(epi_batt_soc_from_mv(3200) == 0, "poniżej pustego");

    for (mv = 3300; mv <= 4200; mv += 10) {
        soc = epi_batt_soc_from_mv(mv);
        CHECK((int)soc >= prev, "SoC nie maleje przy %u mV", mv);
        prev = soc;
    }

    /* 3820 mV to punkt 50 % w tablicy. */
    CHECK(epi_batt_soc_from_mv(3820) == 50, "punkt tablicy 50 %%, jest %u",
          epi_batt_soc_from_mv(3820));

    /* Filtr: chwilowy spadek napięcia pod obciążeniem nie zbija wskazania. */
    epi_batt_init(&b);
    soc = epi_batt_update(&b, 3920, false);
    CHECK(soc == 70, "start od 70 %%, jest %u", soc);
    soc = epi_batt_update(&b, 3700, false); /* impuls wibracji */
    CHECK(soc >= 60, "pojedynczy impuls nie zbija wskazania, jest %u", soc);

    /* Bez ładowania wskazanie nie rośnie. */
    epi_batt_init(&b);
    (void)epi_batt_update(&b, 3800, false);
    soc = b.soc;
    (void)epi_batt_update(&b, 4100, false);
    CHECK(b.soc == soc, "bez ładowarki SoC nie rośnie");
    (void)epi_batt_update(&b, 4100, true);
    CHECK(b.soc > soc, "z ładowarką SoC rośnie");

    /* ADC: dzielnik 1:2, odniesienie 3,0 V, 12 bitów. */
    mv = epi_batt_mv_from_adc(2048, 4095, 3000);
    CHECK(mv > 2990 && mv < 3010, "połowa zakresu ADC = %u mV", mv);
}

static void test_button(void)
{
    epi_button b;
    uint32_t t;
    epi_btn_event e;
    int clicks = 0, longs = 0;

    /* Kliknięcie. */
    epi_button_init(&b, 0);
    for (t = 0; t <= 400; t += 10) {
        e = epi_button_update(&b, t >= 100 && t < 300, t);
        if (e == EPI_BTN_EVT_CLICK) {
            clicks++;
        }
    }
    CHECK(clicks == 1, "jedno kliknięcie, jest %d", clicks);

    /* Drganie styku krótsze niż filtr nie generuje zdarzenia. */
    epi_button_init(&b, 0);
    clicks = 0;
    for (t = 0; t <= 400; t += 5) {
        e = epi_button_update(&b, t >= 100 && t < 115, t);
        if (e != EPI_BTN_EVT_NONE) {
            clicks++;
        }
    }
    CHECK(clicks == 0, "drganie styku odfiltrowane, zdarzeń %d", clicks);

    /* Przytrzymanie: jedno zdarzenie LONG, bez kliknięcia po zwolnieniu. */
    epi_button_init(&b, 0);
    clicks = 0;
    for (t = 0; t <= 4000; t += 10) {
        e = epi_button_update(&b, t >= 100 && t < 3000, t);
        if (e == EPI_BTN_EVT_LONG) {
            longs++;
        }
        if (e == EPI_BTN_EVT_CLICK) {
            clicks++;
        }
    }
    CHECK(longs == 1, "jedno zdarzenie przytrzymania, jest %d", longs);
    CHECK(clicks == 0, "przytrzymanie nie kończy się kliknięciem");

    /* Bardzo długie przytrzymanie: restart, przedtem LONG. */
    epi_button_init(&b, 0);
    longs = 0;
    {
        int resets = 0;
        for (t = 0; t <= 12000; t += 10) {
            e = epi_button_update(&b, t >= 100, t);
            if (e == EPI_BTN_EVT_LONG) {
                longs++;
            }
            if (e == EPI_BTN_EVT_RESET) {
                resets++;
            }
        }
        CHECK(longs == 1 && resets == 1, "LONG=%d RESET=%d", longs, resets);
    }
}

/* Generator ruchu: moduł przyspieszenia = g + amplituda * sin(2*pi*f*t). */
static float shake(float t_s, float freq_hz, float peak)
{
    return 9.81f + peak * sinf(6.2831853f * freq_hz * t_s);
}

static void test_detector_features(void)
{
    epi_detector d;
    float amp = 0.0f, freq = 0.0f;
    uint32_t t;

    epi_det_init(&d, NULL, 0);
    for (t = 0; t <= 4000; t += 15) { /* ~67 Hz */
        epi_det_push_motion(&d, t, shake((float)t / 1000.0f, 3.5f, 4.0f));
    }
    epi_det_features(&d, 4000, 4.0f, &amp, &freq);
    CHECK(fabsf(amp - 2.83f) < 0.2f, "RMS sinusa 4 m/s^2 = %.2f", amp);
    CHECK(fabsf(freq - 3.5f) < 0.2f, "częstotliwość = %.2f Hz", freq);

    /* Drgania poza pasmem napadowym. */
    epi_det_init(&d, NULL, 0);
    for (t = 0; t <= 4000; t += 15) {
        epi_det_push_motion(&d, t, shake((float)t / 1000.0f, 8.0f, 4.0f));
    }
    epi_det_features(&d, 4000, 4.0f, &amp, &freq);
    CHECK(freq > 7.0f, "8 Hz rozpoznane jako %.2f Hz", freq);

    /* Spoczynek. */
    epi_det_init(&d, NULL, 0);
    for (t = 0; t <= 4000; t += 15) {
        epi_det_push_motion(&d, t, 9.81f);
    }
    epi_det_features(&d, 4000, 4.0f, &amp, &freq);
    CHECK(amp < 0.01f, "spoczynek: amplituda = %.3f", amp);
}

/* Przebieg: drgania w paśmie + rosnące tętno -> ALARM. */
static void test_detector_alarm(void)
{
    epi_detector d;
    uint32_t t;
    bool saw_suspect = false, saw_confirming = false;
    epi_det_state st = EPI_DET_IDLE;

    epi_det_init(&d, NULL, 0);
    for (t = 0; t <= 25000; t += 15) {
        epi_det_push_motion(&d, t, shake((float)t / 1000.0f, 3.5f, 4.0f));
        if (t % 1000u == 0u) {
            float hr = 70.0f + 3.5f * ((float)t / 1000.0f);
            epi_det_push_vitals(&d, t, hr, 97.0f);
        }
        if (t % 255u < 15u) { /* mniej więcej co 250 ms, jak w aplikacji */
            st = epi_det_tick(&d, t);
            if (st == EPI_DET_SUSPECT) {
                saw_suspect = true;
            }
            if (st == EPI_DET_CONFIRMING) {
                saw_confirming = true;
            }
            if (st == EPI_DET_ALARM) {
                break;
            }
        }
    }
    CHECK(saw_suspect, "stan SUSPECT wystąpił");
    CHECK(saw_confirming, "stan CONFIRMING wystąpił");
    CHECK(st == EPI_DET_ALARM, "alarm podniesiony, stan = %s",
          epi_det_state_name(st));
    CHECK(t >= 18000 && t <= 20000,
          "alarm po 3 s utrzymania i 15 s okna, jest %u ms", t);
    CHECK(d.an.hr_pass, "kryterium tętna spełnione (slope=%.2f)", d.an.hr_slope);

    /* Kasowanie alarmu przyciskiem uruchamia wyciszenie. */
    epi_det_cancel(&d, t);
    CHECK(d.state == EPI_DET_IDLE, "po skasowaniu stan IDLE");
    CHECK(d.cooldown_until_ms > t, "wyciszenie ustawione");
}

/* Drgania ustają w oknie potwierdzenia -> zdarzenie odrzucone. */
static void test_detector_reject(void)
{
    epi_detector d;
    uint32_t t;
    bool rejected = false;
    epi_det_state st;

    epi_det_init(&d, NULL, 0);
    for (t = 0; t <= 20000; t += 15) {
        float mag = (t < 6000u) ? shake((float)t / 1000.0f, 3.5f, 4.0f) : 9.81f;
        epi_det_push_motion(&d, t, mag);
        if (t % 1000u == 0u) {
            epi_det_push_vitals(&d, t, 72.0f, 97.0f);
        }
        if (t % 255u < 15u) {
            st = epi_det_tick(&d, t);
            if (st == EPI_DET_REJECTED) {
                rejected = true;
                break;
            }
        }
    }
    CHECK(rejected, "brak wzorca w oknie potwierdzenia kończy się odrzuceniem");
    CHECK(d.cooldown_until_ms > t, "po odrzuceniu obowiązuje wyciszenie");

    /* W czasie wyciszenia te same drgania nie budzą automatu. */
    for (; t <= 40000; t += 15) {
        epi_det_push_motion(&d, t, shake((float)t / 1000.0f, 3.5f, 4.0f));
        if (t % 255u < 15u) {
            st = epi_det_tick(&d, t);
            CHECK(st == EPI_DET_IDLE || st == EPI_DET_REJECTED,
                  "w wyciszeniu stan = %s", epi_det_state_name(st));
            if (t > 30000u) {
                break;
            }
        }
    }
}

/* Brak kryterium biometrycznego: intensywny ruch bez zmian tętna i SpO2. */
static void test_detector_motion_only(void)
{
    epi_detector d;
    uint32_t t;
    epi_det_state st = EPI_DET_IDLE;
    bool alarm = false;

    epi_det_init(&d, NULL, 0);
    for (t = 0; t <= 25000; t += 15) {
        epi_det_push_motion(&d, t, shake((float)t / 1000.0f, 3.5f, 4.0f));
        if (t % 1000u == 0u) {
            epi_det_push_vitals(&d, t, 72.0f, 97.0f);
        }
        if (t % 255u < 15u) {
            st = epi_det_tick(&d, t);
            if (st == EPI_DET_ALARM) {
                alarm = true;
                break;
            }
        }
    }
    CHECK(!alarm, "sam ruch nie wystarcza do alarmu, stan = %s",
          epi_det_state_name(st));
}

int main(void)
{
    test_led_priority();
    test_led_soc_color();
    test_led_patterns();
    test_battery();
    test_button();
    test_detector_features();
    test_detector_alarm();
    test_detector_reject();
    test_detector_motion_only();

    printf("%d sprawdzeń, %d niepowodzeń\n", checks, failures);
    return failures ? 1 : 0;
}
