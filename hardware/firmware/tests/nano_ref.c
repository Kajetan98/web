#include "nano_ref.h"

#include <stddef.h>

#include "epi_battery.h"
#include "epi_button.h"
#include "epi_detector.h"
#include "epi_led.h"

void ref_soc_color(uint8_t soc, uint8_t out[3])
{
    epi_rgb c = epi_led_soc_color(soc);
    out[0] = c.r;
    out[1] = c.g;
    out[2] = c.b;
}

uint8_t ref_soc_from_mv(uint16_t mv)
{
    return epi_batt_soc_from_mv(mv);
}

static epi_led_input make_input(int powered, int charger, int complete,
                                int fault, int sos, uint8_t soc, int det_state,
                                uint32_t uptime_ms, uint8_t brightness)
{
    epi_led_input in;

    in.powered_on = powered != 0;
    in.charger_present = charger != 0;
    in.charge_complete = complete != 0;
    in.fault = fault != 0;
    in.sos_active = sos != 0;
    in.soc = soc;
    in.detector = (epi_det_state)det_state;
    in.uptime_ms = uptime_ms;
    in.brightness = brightness;
    return in;
}

int ref_led_mode(int powered, int charger, int complete, int fault, int sos,
                 uint8_t soc, int det_state, uint32_t uptime_ms)
{
    epi_led_input in = make_input(powered, charger, complete, fault, sos, soc,
                                  det_state, uptime_ms, 255);
    return (int)epi_led_mode_for(&in);
}

void ref_led_render(int powered, int charger, int complete, int fault, int sos,
                    uint8_t soc, int det_state, uint32_t uptime_ms,
                    uint8_t brightness, uint32_t now_ms, uint8_t out[3])
{
    epi_led_input in = make_input(powered, charger, complete, fault, sos, soc,
                                  det_state, uptime_ms, brightness);
    epi_rgb c = epi_led_render(&in, now_ms);

    out[0] = c.r;
    out[1] = c.g;
    out[2] = c.b;
}

static epi_button btn;

void ref_button_init(uint32_t now_ms)
{
    epi_button_init(&btn, now_ms);
}

int ref_button_update(int pressed, uint32_t now_ms)
{
    switch (epi_button_update(&btn, pressed != 0, now_ms)) {
    case EPI_BTN_EVT_CLICK: return 1;
    case EPI_BTN_EVT_LONG:  return 2;
    case EPI_BTN_EVT_RESET: return 3;
    default:                return 0;
    }
}

static epi_detector det;

void ref_det_init(void)
{
    epi_det_init(&det, NULL, 0);
}

void ref_det_push_motion(uint32_t t_ms, float mag)
{
    epi_det_push_motion(&det, t_ms, mag);
}

void ref_det_push_vitals(uint32_t t_ms, float hr, float spo2)
{
    epi_det_push_vitals(&det, t_ms, hr, spo2);
}

int ref_det_tick(uint32_t t_ms, float *amp, float *freq)
{
    epi_det_state s = epi_det_tick(&det, t_ms);

    *amp = det.an.amp;
    *freq = det.an.freq;
    return (int)s;
}
