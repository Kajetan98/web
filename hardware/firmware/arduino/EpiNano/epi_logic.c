#include "epi_logic.h"

#include <math.h>

/* Kalibracja kanałów diody — te same wartości co w firmware/lib/epi_led.c. */
#define GAIN_R 255
#define GAIN_G 115
#define GAIN_B 140

#define LOST_SUSPECT_MS  1500u
#define LOST_CONFIRM_MS  3000u
#define SLOPE_SPAN_MS   20000u
#define BTN_DEBOUNCE_MS    30u
#define BTN_CLICK_MAX_MS  700u
#define BTN_LONG_MS      2000u
#define BTN_RESET_MS     8000u

/* --- konfiguracja ------------------------------------------------------ */

void epi_cfg_defaults(epi_cfg *cfg)
{
    cfg->amp_min = 2.5f;
    cfg->freq_min = 2.5f;
    cfg->freq_max = 5.5f;
    cfg->hold_s = 3.0f;
    cfg->window_s = 15.0f;
    cfg->hr_rise_pct = 35.0f;
    cfg->hr_slope_bpm_s = 1.2f;
    cfg->spo2_drop_pp = 4.0f;
    cfg->require_both = false;
    cfg->require_bio = true;
    cfg->cooldown_s = 45.0f;
}

void epi_init(epi_ctx *c, const epi_cfg *cfg, uint32_t now_ms)
{
    uint8_t i;

    if (cfg) {
        c->cfg = *cfg;
    } else {
        epi_cfg_defaults(&c->cfg);
    }
    c->state = EPI_IDLE;
    c->an.amp = 0.0f;
    c->an.freq = 0.0f;
    c->an.hr_slope = 0.0f;
    c->an.motion_pass = false;
    c->an.hr_pass = false;
    c->an.spo2_pass = false;

    for (i = 0; i < EPI_N_CAP; i++) {
        c->mag[i] = 0;
    }
    c->head = 0;
    c->len = 0;
    for (i = 0; i < EPI_HR_CAP; i++) {
        c->hr_hist[i] = 0;
        c->hr_t[i] = 0;
    }
    c->hr_head = 0;
    c->hr_len = 0;

    c->baseline_hr = 68.0f;
    c->baseline_spo2 = 97.0f;
    c->hr = c->baseline_hr;
    c->spo2 = c->baseline_spo2;

    c->pattern_since_ms = 0;
    c->pattern_lost_ms = 0;
    c->confirm_until_ms = 0;
    c->cooldown_until_ms = 0;
    c->boot_ms = now_ms;

    c->btn_stable = false;
    c->btn_raw_last = false;
    c->btn_raw_since_ms = now_ms;
    c->btn_press_start_ms = 0;
    c->btn_long_sent = false;
    c->btn_reset_sent = false;

    c->mv_filtered = 0;
    c->soc = 100;
    c->batt_primed = false;
    c->charger_present = false;
    c->charge_complete = false;
    c->powered_on = true;
    c->sos_active = false;
    c->fault = false;
    c->brightness = 60;
}

/* --- tor ruchowy ------------------------------------------------------- */

void epi_push_accel(epi_ctx *c, float mag_ms2)
{
    float v = mag_ms2 * 100.0f;

    if (v > 32000.0f) {
        v = 32000.0f;
    }
    if (v < -32000.0f) {
        v = -32000.0f;
    }
    c->mag[c->head] = (int16_t)v;
    c->head = (uint8_t)((c->head + 1u) % EPI_N_CAP);
    if (c->len < EPI_N_CAP) {
        c->len++;
    }
}

void epi_push_vitals(epi_ctx *c, uint32_t now_ms, float hr, float spo2)
{
    if (hr > 0.0f) {
        c->hr = hr;
        c->hr_hist[c->hr_head] = (int16_t)(hr + 0.5f);
        c->hr_t[c->hr_head] = now_ms;
        c->hr_head = (uint8_t)((c->hr_head + 1u) % EPI_HR_CAP);
        if (c->hr_len < EPI_HR_CAP) {
            c->hr_len++;
        }
    }
    if (spo2 > 0.0f) {
        c->spo2 = spo2;
    }
}

/* Amplituda RMS i częstotliwość dominująca z przejść przez zero, liczone
 * w oknie EPI_WINDOW_S. Czas trwania okna wynika ze stałej częstotliwości
 * próbkowania, bo pamiętanie znaczników czasu kosztowałoby cztery razy
 * więcej pamięci niż same próbki. */
void epi_features(const epi_ctx *c, float *amp, float *freq)
{
    /* Okno obejmuje próbki od chwili now-EPI_WINDOW_S do now włącznie,
     * czyli o jedną więcej niż iloczyn częstotliwości i czasu; tylko
     * wtedy czas trwania okna wychodzi dokładnie EPI_WINDOW_S. */
    uint8_t want = (uint8_t)(EPI_FS_HZ * EPI_WINDOW_S + 1u);
    uint8_t count = (c->len < want) ? c->len : want;
    uint8_t start, i;
    int32_t sum = 0;
    float mean, sq = 0.0f, prev = 0.0f, span;
    uint16_t crossings = 0;

    *amp = 0.0f;
    *freq = 0.0f;
    if (count < 16u) {
        return;
    }
    start = (uint8_t)((c->head + EPI_N_CAP - count) % EPI_N_CAP);

    for (i = 0; i < count; i++) {
        sum += c->mag[(uint8_t)((start + i) % EPI_N_CAP)];
    }
    /* Odjęcie średniej okna usuwa składową grawitacyjną. */
    mean = (float)sum / (float)count;

    for (i = 0; i < count; i++) {
        float dev = (float)c->mag[(uint8_t)((start + i) % EPI_N_CAP)] - mean;
        if (i > 0u) {
            sq += dev * dev;
            if ((prev < 0.0f && dev >= 0.0f) || (prev > 0.0f && dev <= 0.0f)) {
                crossings++;
            }
        }
        prev = dev;
    }
    span = (float)(count - 1u) / (float)EPI_FS_HZ;
    *amp = sqrtf(sq / (float)(count - 1u)) / 100.0f;
    *freq = (float)crossings / (2.0f * span);
}

static float slope_bpm_s(const epi_ctx *c, uint32_t now_ms)
{
    uint8_t i, first = 0xFFu, last = 0, count = 0;
    uint32_t from = (now_ms > SLOPE_SPAN_MS) ? (now_ms - SLOPE_SPAN_MS) : 0u;
    float dt;

    for (i = 0; i < c->hr_len; i++) {
        uint8_t idx = (uint8_t)((c->hr_head + EPI_HR_CAP - c->hr_len + i) % EPI_HR_CAP);
        if (c->hr_t[idx] >= from) {
            if (first == 0xFFu) {
                first = idx;
            }
            last = idx;
            count++;
        }
    }
    if (count < 4u) {
        return 0.0f;
    }
    dt = (float)(c->hr_t[last] - c->hr_t[first]) / 1000.0f;
    if (dt <= 3.0f) {
        return 0.0f;
    }
    return (float)(c->hr_hist[last] - c->hr_hist[first]) / dt;
}

static bool bio_ok(const epi_ctx *c)
{
    if (!c->cfg.require_bio) {
        return true; /* tryb pokazowy bez pulsoksymetru */
    }
    return c->cfg.require_both ? (c->an.hr_pass && c->an.spo2_pass)
                               : (c->an.hr_pass || c->an.spo2_pass);
}

static void resolve(epi_ctx *c, epi_state s, uint32_t now_ms)
{
    c->cooldown_until_ms = now_ms + (uint32_t)(c->cfg.cooldown_s * 1000.0f);
    c->pattern_since_ms = 0;
    c->pattern_lost_ms = 0;
    c->state = s;
}

epi_state epi_tick(epi_ctx *c, uint32_t now_ms)
{
    if (c->state == EPI_OFF) {
        return c->state;
    }

    epi_features(c, &c->an.amp, &c->an.freq);
    c->an.motion_pass = (c->an.amp >= c->cfg.amp_min) &&
                        (c->an.freq >= c->cfg.freq_min) &&
                        (c->an.freq <= c->cfg.freq_max);
    c->an.hr_slope = slope_bpm_s(c, now_ms);
    c->an.hr_pass = (c->hr >= c->baseline_hr * (1.0f + c->cfg.hr_rise_pct / 100.0f)) &&
                    (c->an.hr_slope >= c->cfg.hr_slope_bpm_s);
    c->an.spo2_pass = (c->spo2 <= c->baseline_spo2 - c->cfg.spo2_drop_pp);

    switch (c->state) {
    case EPI_REJECTED:
        c->state = EPI_IDLE;
        break;
    case EPI_ALARM:
        break; /* alarm trwa do skasowania przyciskiem albo komendą */
    case EPI_IDLE:
    case EPI_SUSPECT:
        if (c->an.motion_pass && now_ms >= c->cooldown_until_ms) {
            if (c->pattern_since_ms == 0u) {
                c->pattern_since_ms = now_ms;
            }
            c->pattern_lost_ms = 0;
            c->state = EPI_SUSPECT;
            if ((float)(now_ms - c->pattern_since_ms) / 1000.0f >= c->cfg.hold_s) {
                c->confirm_until_ms = now_ms + (uint32_t)(c->cfg.window_s * 1000.0f);
                c->pattern_lost_ms = 0;
                c->state = EPI_CONFIRMING;
            }
        } else if (c->pattern_since_ms != 0u) {
            if (c->pattern_lost_ms == 0u) {
                c->pattern_lost_ms = now_ms;
            }
            if (now_ms - c->pattern_lost_ms > LOST_SUSPECT_MS) {
                c->pattern_since_ms = 0;
                c->pattern_lost_ms = 0;
                c->state = EPI_IDLE;
            }
        }
        /* Powolny dryf linii bazowej, tylko gdy nic się nie dzieje. */
        if (c->state == EPI_IDLE && c->an.amp <= 0.6f) {
            if (c->hr > 30.0f && c->hr < 130.0f) {
                c->baseline_hr = c->baseline_hr * 0.995f + c->hr * 0.005f;
            }
            if (c->spo2 > 85.0f) {
                c->baseline_spo2 = c->baseline_spo2 * 0.995f + c->spo2 * 0.005f;
            }
        }
        break;
    case EPI_CONFIRMING:
        if (!c->an.motion_pass) {
            if (c->pattern_lost_ms == 0u) {
                c->pattern_lost_ms = now_ms;
            }
            if (now_ms - c->pattern_lost_ms > LOST_CONFIRM_MS) {
                resolve(c, EPI_REJECTED, now_ms);
                break;
            }
        } else {
            c->pattern_lost_ms = 0;
        }
        if (now_ms >= c->confirm_until_ms) {
            resolve(c, bio_ok(c) ? EPI_ALARM : EPI_REJECTED, now_ms);
        }
        break;
    default:
        break;
    }
    return c->state;
}

void epi_cancel(epi_ctx *c, uint32_t now_ms)
{
    c->sos_active = false;
    if (c->state == EPI_CONFIRMING || c->state == EPI_ALARM) {
        resolve(c, EPI_IDLE, now_ms);
    }
}

/* --- przycisk ---------------------------------------------------------- */

epi_btn_event epi_btn_poll(epi_ctx *c, bool pressed, uint32_t now_ms)
{
    epi_btn_event evt = EPI_BTN_NONE;

    if (pressed != c->btn_raw_last) {
        c->btn_raw_last = pressed;
        c->btn_raw_since_ms = now_ms;
    }

    if (pressed != c->btn_stable &&
        (now_ms - c->btn_raw_since_ms) >= BTN_DEBOUNCE_MS) {
        c->btn_stable = pressed;
        if (pressed) {
            c->btn_press_start_ms = c->btn_raw_since_ms;
            c->btn_long_sent = false;
            c->btn_reset_sent = false;
        } else if (!c->btn_long_sent && !c->btn_reset_sent) {
            if ((c->btn_raw_since_ms - c->btn_press_start_ms) <= BTN_CLICK_MAX_MS) {
                evt = EPI_BTN_CLICK;
            }
        }
        return evt;
    }

    if (c->btn_stable) {
        uint32_t held = now_ms - c->btn_press_start_ms;
        if (!c->btn_reset_sent && held >= BTN_RESET_MS) {
            c->btn_reset_sent = true;
            return EPI_BTN_RESET;
        }
        if (!c->btn_long_sent && held >= BTN_LONG_MS) {
            c->btn_long_sent = true;
            return EPI_BTN_LONG;
        }
    }
    return EPI_BTN_NONE;
}

/* --- bateria ----------------------------------------------------------- */

static const uint16_t ocv_mv[11] = {
    4200, 4060, 3980, 3920, 3870, 3820, 3790, 3770, 3740, 3680, 3300
};

uint8_t epi_soc_from_mv(uint16_t mv)
{
    uint8_t i;

    if (mv >= ocv_mv[0]) {
        return 100u;
    }
    if (mv <= ocv_mv[10]) {
        return 0u;
    }
    for (i = 0; i < 10u; i++) {
        uint16_t hi = ocv_mv[i];
        uint16_t lo = ocv_mv[i + 1u];
        if (mv <= hi && mv > lo) {
            uint8_t soc_hi = (uint8_t)(100u - i * 10u);
            uint16_t span = (uint16_t)(hi - lo);
            uint32_t frac = ((uint32_t)(mv - lo) * 10u) / (span ? span : 1u);
            return (uint8_t)((soc_hi - 10u) + frac);
        }
    }
    return 0u;
}

uint8_t epi_batt_tick(epi_ctx *c, uint16_t mv)
{
    uint8_t soc;

    if (!c->batt_primed) {
        c->mv_filtered = mv;
        c->batt_primed = true;
        c->soc = epi_soc_from_mv(mv);
        return c->soc;
    }
    c->mv_filtered = (uint16_t)(((uint32_t)c->mv_filtered * 7u + mv) / 8u);
    soc = epi_soc_from_mv(c->mv_filtered);

    if (c->charger_present) {
        if (soc > c->soc) {
            c->soc = soc;
        }
    } else if (soc < c->soc) {
        c->soc = soc;
    }
    return c->soc;
}

/* --- dioda ------------------------------------------------------------- */

static uint8_t scale8(uint8_t value, uint8_t factor)
{
    return (uint8_t)(((uint16_t)value * (uint16_t)factor + 127u) / 255u);
}

static void hue_rgb(uint16_t hue_deg, uint8_t out[3])
{
    uint16_t h = (uint16_t)(hue_deg % 360u);
    uint8_t sector = (uint8_t)(h / 60u);
    uint8_t rise = (uint8_t)(((uint32_t)(h % 60u) * 255u) / 60u);
    uint8_t fall = (uint8_t)(255u - rise);

    switch (sector) {
    case 0: out[0] = 255;  out[1] = rise; out[2] = 0;    break;
    case 1: out[0] = fall; out[1] = 255;  out[2] = 0;    break;
    case 2: out[0] = 0;    out[1] = 255;  out[2] = rise; break;
    case 3: out[0] = 0;    out[1] = fall; out[2] = 255;  break;
    case 4: out[0] = rise; out[1] = 0;    out[2] = 255;  break;
    default: out[0] = 255; out[1] = 0;    out[2] = fall; break;
    }
    out[0] = scale8(out[0], GAIN_R);
    out[1] = scale8(out[1], GAIN_G);
    out[2] = scale8(out[2], GAIN_B);
}

void epi_soc_color(uint8_t soc, uint8_t out[3])
{
    uint16_t hue;

    if (soc > 100u) {
        soc = 100u;
    }
    if (soc <= EPI_SOC_RED) {
        hue = 0u;
    } else {
        hue = (uint16_t)(((uint32_t)(soc - EPI_SOC_RED) * 120u) /
                         (100u - EPI_SOC_RED));
    }
    hue_rgb(hue, out);
}

epi_led_mode epi_led_mode_of(const epi_ctx *c, uint32_t now_ms)
{
    if (!c->powered_on && !c->charger_present) {
        return EPI_LED_OFF;
    }
    if (c->sos_active || c->state == EPI_ALARM) {
        return EPI_LED_ALARM;
    }
    if (c->state == EPI_CONFIRMING) {
        return EPI_LED_CONFIRMING;
    }
    if (c->charger_present) {
        return c->charge_complete ? EPI_LED_CHARGED : EPI_LED_CHARGING;
    }
    if (c->fault) {
        return EPI_LED_FAULT;
    }
    if ((now_ms - c->boot_ms) < EPI_BOOT_MS) {
        return EPI_LED_BOOT;
    }
    if (c->soc <= EPI_SOC_RED) {
        return EPI_LED_LOW_BATTERY;
    }
    return EPI_LED_MONITOR;
}

static uint8_t gamma8(uint8_t v)
{
    return (uint8_t)(((uint16_t)v * (uint16_t)v + 255u) / 256u);
}

/* Wzorzec: okres, długość błysku, odstęp w serii, liczba błysków, oddech. */
static void pattern_of(epi_led_mode m, uint16_t *period, uint16_t *on,
                       uint16_t *gap, uint8_t *pulses, bool *breathe)
{
    *gap = 0;
    *pulses = 1;
    *breathe = false;
    switch (m) {
    case EPI_LED_ALARM:       *period = 250;  *on = 125;  break;
    case EPI_LED_CONFIRMING:  *period = 500;  *on = 120;  break;
    case EPI_LED_CHARGED:     *period = 0;    *on = 0;    break;
    case EPI_LED_CHARGING:    *period = 3000; *on = 3000; *breathe = true; break;
    case EPI_LED_FAULT:       *period = 5000; *on = 60; *gap = 200; *pulses = 3; break;
    case EPI_LED_BOOT:        *period = 0;    *on = 0;    break;
    case EPI_LED_LOW_BATTERY: *period = 5000; *on = 40; *gap = 220; *pulses = 2; break;
    case EPI_LED_MONITOR:     *period = 5000; *on = 30;  break;
    default:                  *period = 1000; *on = 0;   *pulses = 0; break;
    }
}

void epi_led_rgb(const epi_ctx *c, uint32_t now_ms, uint8_t out[3])
{
    epi_led_mode m = epi_led_mode_of(c, now_ms);
    uint16_t period, on, gap;
    uint8_t pulses, env = 0, level, i;
    bool breathe;

    pattern_of(m, &period, &on, &gap, &pulses, &breathe);

    switch (m) {
    case EPI_LED_ALARM:
    case EPI_LED_LOW_BATTERY: hue_rgb(0, out); break;
    case EPI_LED_CONFIRMING:  hue_rgb(35, out); break;
    case EPI_LED_CHARGED:     hue_rgb(120, out); break;
    case EPI_LED_CHARGING:    hue_rgb(225, out); break;
    case EPI_LED_FAULT:       hue_rgb(300, out); break;
    case EPI_LED_MONITOR:
    case EPI_LED_BOOT:        epi_soc_color(c->soc, out); break;
    default:                  out[0] = out[1] = out[2] = 0; return;
    }

    if (pulses == 0u) {
        env = 0u;
    } else if (period == 0u) {
        env = 255u;
    } else {
        uint32_t phase = now_ms % period;
        if (breathe) {
            uint32_t half = period / 2u;
            uint32_t up = (phase < half) ? phase : (period - phase);
            env = gamma8((uint8_t)((up * 255u) / (half ? half : 1u)));
        } else {
            uint32_t slot = (uint32_t)on + (uint32_t)gap;
            for (i = 0; i < pulses; i++) {
                uint32_t s = (uint32_t)i * slot;
                if (phase >= s && phase < s + (uint32_t)on) {
                    env = 255u;
                    break;
                }
            }
        }
    }

    if (env == 0u) {
        out[0] = out[1] = out[2] = 0;
        return;
    }
    level = (m == EPI_LED_ALARM) ? 255u : c->brightness;
    level = scale8(level, env);
    for (i = 0; i < 3u; i++) {
        out[i] = scale8(out[i], level);
    }
}

const char *epi_state_name(epi_state s)
{
    switch (s) {
    case EPI_OFF:        return "OFF";
    case EPI_IDLE:       return "IDLE";
    case EPI_SUSPECT:    return "SUSPECT";
    case EPI_CONFIRMING: return "CONFIRMING";
    case EPI_ALARM:      return "ALARM";
    case EPI_REJECTED:   return "REJECTED";
    default:             return "?";
    }
}
