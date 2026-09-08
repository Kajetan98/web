#include "epi_detector.h"

#include <math.h>

/* Aplikacja liczy analizę co 250 ms w oknie 4 s — te same wartości. */
#define EPI_DET_WINDOW_S      4.0f
#define EPI_DET_MIN_SAMPLES   16u
#define EPI_DET_LOST_SUSPECT  1500u /* ms bez wzorca, zanim SUSPECT wraca do IDLE */
#define EPI_DET_LOST_CONFIRM  3000u /* ms bez wzorca w oknie potwierdzenia */
#define EPI_DET_SLOPE_SPAN_MS 20000u

void epi_det_defaults(epi_det_cfg *cfg)
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
    cfg->cooldown_s = 45.0f;
}

void epi_det_init(epi_detector *d, const epi_det_cfg *cfg, uint32_t now_ms)
{
    uint16_t i;

    if (cfg) {
        d->cfg = *cfg;
    } else {
        epi_det_defaults(&d->cfg);
    }
    d->state = EPI_DET_IDLE;
    d->an.amp = 0.0f;
    d->an.freq = 0.0f;
    d->an.hr_slope = 0.0f;
    d->an.motion_pass = false;
    d->an.hr_pass = false;
    d->an.spo2_pass = false;
    d->baseline_hr = 68.0f;
    d->baseline_spo2 = 97.0f;
    d->hr = d->baseline_hr;
    d->spo2 = d->baseline_spo2;
    d->motion_head = 0;
    d->motion_len = 0;
    d->hr_head = 0;
    d->hr_len = 0;
    d->pattern_since_ms = 0;
    d->pattern_lost_ms = 0;
    d->confirm_until_ms = 0;
    d->cooldown_until_ms = 0;
    d->state_since_ms = now_ms;

    for (i = 0; i < EPI_MOTION_CAP; i++) {
        d->motion[i].t_ms = 0;
        d->motion[i].mag = 0.0f;
    }
}

void epi_det_push_motion(epi_detector *d, uint32_t t_ms, float mag)
{
    d->motion[d->motion_head].t_ms = t_ms;
    d->motion[d->motion_head].mag = mag;
    d->motion_head = (uint16_t)((d->motion_head + 1u) % EPI_MOTION_CAP);
    if (d->motion_len < EPI_MOTION_CAP) {
        d->motion_len++;
    }
}

void epi_det_push_vitals(epi_detector *d, uint32_t t_ms, float hr, float spo2)
{
    if (hr > 0.0f) {
        d->hr = hr;
        d->hr_hist[d->hr_head].t_ms = t_ms;
        d->hr_hist[d->hr_head].hr = hr;
        d->hr_head = (uint8_t)((d->hr_head + 1u) % EPI_HR_CAP);
        if (d->hr_len < EPI_HR_CAP) {
            d->hr_len++;
        }
    }
    if (spo2 > 0.0f) {
        d->spo2 = spo2;
    }
}

/* Indeks i-tej najstarszej próbki w buforze cyklicznym. */
static const epi_motion_sample *motion_at(const epi_detector *d, uint16_t i)
{
    uint16_t start = (uint16_t)((d->motion_head + EPI_MOTION_CAP - d->motion_len) %
                                EPI_MOTION_CAP);
    return &d->motion[(uint16_t)((start + i) % EPI_MOTION_CAP)];
}

void epi_det_features(const epi_detector *d, uint32_t now_ms, float window_s,
                      float *amp, float *freq)
{
    uint32_t from;
    uint16_t i, first = 0, count = 0;
    float sum = 0.0f, sq = 0.0f, prev = 0.0f, span;
    uint16_t crossings = 0;
    bool have_prev = false;

    *amp = 0.0f;
    *freq = 0.0f;
    if (d->motion_len == 0u) {
        return;
    }
    from = (uint32_t)(window_s * 1000.0f);
    from = (now_ms > from) ? (now_ms - from) : 0u;

    for (i = 0; i < d->motion_len; i++) {
        if (motion_at(d, i)->t_ms >= from) {
            first = i;
            count = (uint16_t)(d->motion_len - i);
            break;
        }
    }
    if (count < EPI_DET_MIN_SAMPLES) {
        return;
    }

    for (i = 0; i < count; i++) {
        sum += motion_at(d, (uint16_t)(first + i))->mag;
    }
    /* Odjęcie średniej okna usuwa składową grawitacyjną. */
    sum /= (float)count;

    for (i = 0; i < count; i++) {
        float dev = motion_at(d, (uint16_t)(first + i))->mag - sum;
        if (i > 0u) {
            sq += dev * dev;
            if ((prev < 0.0f && dev >= 0.0f) || (prev > 0.0f && dev <= 0.0f)) {
                crossings++;
            }
        }
        prev = dev;
        have_prev = true;
    }
    (void)have_prev;

    span = (float)(motion_at(d, (uint16_t)(first + count - 1u))->t_ms -
                   motion_at(d, first)->t_ms) / 1000.0f;
    if (span <= 0.0f) {
        span = window_s;
    }
    *amp = sqrtf(sq / (float)(count - 1u));
    *freq = (float)crossings / (2.0f * span);
}

static bool bio_confirmed(const epi_detector *d)
{
    return d->cfg.require_both ? (d->an.hr_pass && d->an.spo2_pass)
                               : (d->an.hr_pass || d->an.spo2_pass);
}

static float hr_slope(const epi_detector *d, uint32_t now_ms)
{
    uint8_t i, first = 0xFFu, last = 0, count = 0;
    uint32_t from = (now_ms > EPI_DET_SLOPE_SPAN_MS)
                        ? (now_ms - EPI_DET_SLOPE_SPAN_MS) : 0u;
    float dt;

    for (i = 0; i < d->hr_len; i++) {
        uint8_t idx = (uint8_t)((d->hr_head + EPI_HR_CAP - d->hr_len + i) % EPI_HR_CAP);
        if (d->hr_hist[idx].t_ms >= from) {
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
    dt = (float)(d->hr_hist[last].t_ms - d->hr_hist[first].t_ms) / 1000.0f;
    if (dt <= 3.0f) {
        return 0.0f;
    }
    return (d->hr_hist[last].hr - d->hr_hist[first].hr) / dt;
}

static void evaluate(epi_detector *d, uint32_t now_ms)
{
    epi_det_features(d, now_ms, EPI_DET_WINDOW_S, &d->an.amp, &d->an.freq);
    d->an.motion_pass = (d->an.amp >= d->cfg.amp_min) &&
                        (d->an.freq >= d->cfg.freq_min) &&
                        (d->an.freq <= d->cfg.freq_max);
    d->an.hr_slope = hr_slope(d, now_ms);
    d->an.hr_pass = (d->hr >= d->baseline_hr * (1.0f + d->cfg.hr_rise_pct / 100.0f)) &&
                    (d->an.hr_slope >= d->cfg.hr_slope_bpm_s);
    d->an.spo2_pass = (d->spo2 <= d->baseline_spo2 - d->cfg.spo2_drop_pp);
}

/* Powolny dryf linii bazowej, tylko gdy nic się nie dzieje. Współczynnik
 * dobrany do wywołań co 250 ms. */
static void learn_baseline(epi_detector *d)
{
    if (d->an.amp > 0.6f) {
        return;
    }
    if (d->hr > 30.0f && d->hr < 130.0f) {
        d->baseline_hr = d->baseline_hr * 0.995f + d->hr * 0.005f;
    }
    if (d->spo2 > 85.0f) {
        d->baseline_spo2 = d->baseline_spo2 * 0.995f + d->spo2 * 0.005f;
    }
}

static void set_state(epi_detector *d, epi_det_state s, uint32_t now_ms)
{
    if (d->state == s) {
        return;
    }
    d->state = s;
    d->state_since_ms = now_ms;
}

static void resolve(epi_detector *d, epi_det_state s, uint32_t now_ms)
{
    d->cooldown_until_ms = now_ms + (uint32_t)(d->cfg.cooldown_s * 1000.0f);
    d->pattern_since_ms = 0;
    d->pattern_lost_ms = 0;
    set_state(d, s, now_ms);
}

epi_det_state epi_det_tick(epi_detector *d, uint32_t now_ms)
{
    if (d->state == EPI_DET_OFF) {
        return d->state;
    }

    evaluate(d, now_ms);

    switch (d->state) {
    case EPI_DET_REJECTED:
        set_state(d, EPI_DET_IDLE, now_ms);
        break;
    case EPI_DET_ALARM:
        break; /* alarm trwa do skasowania przyciskiem */
    case EPI_DET_IDLE:
    case EPI_DET_SUSPECT:
        if (d->an.motion_pass && now_ms >= d->cooldown_until_ms) {
            if (d->pattern_since_ms == 0u) {
                d->pattern_since_ms = now_ms;
            }
            d->pattern_lost_ms = 0;
            set_state(d, EPI_DET_SUSPECT, now_ms);
            if ((float)(now_ms - d->pattern_since_ms) / 1000.0f >= d->cfg.hold_s) {
                d->confirm_until_ms = now_ms + (uint32_t)(d->cfg.window_s * 1000.0f);
                d->pattern_lost_ms = 0;
                set_state(d, EPI_DET_CONFIRMING, now_ms);
            }
        } else if (d->pattern_since_ms != 0u) {
            if (d->pattern_lost_ms == 0u) {
                d->pattern_lost_ms = now_ms;
            }
            if (now_ms - d->pattern_lost_ms > EPI_DET_LOST_SUSPECT) {
                d->pattern_since_ms = 0;
                d->pattern_lost_ms = 0;
                set_state(d, EPI_DET_IDLE, now_ms);
            }
        }
        if (d->state == EPI_DET_IDLE) {
            learn_baseline(d);
        }
        break;
    case EPI_DET_CONFIRMING:
        if (!d->an.motion_pass) {
            if (d->pattern_lost_ms == 0u) {
                d->pattern_lost_ms = now_ms;
            }
            if (now_ms - d->pattern_lost_ms > EPI_DET_LOST_CONFIRM) {
                resolve(d, EPI_DET_REJECTED, now_ms);
                break;
            }
        } else {
            d->pattern_lost_ms = 0;
        }
        if (now_ms >= d->confirm_until_ms) {
            resolve(d, bio_confirmed(d) ? EPI_DET_ALARM : EPI_DET_REJECTED, now_ms);
        }
        break;
    default:
        break;
    }
    return d->state;
}

void epi_det_cancel(epi_detector *d, uint32_t now_ms)
{
    if (d->state == EPI_DET_CONFIRMING || d->state == EPI_DET_ALARM) {
        resolve(d, EPI_DET_IDLE, now_ms);
    }
}

const char *epi_det_state_name(epi_det_state s)
{
    switch (s) {
    case EPI_DET_OFF:        return "OFF";
    case EPI_DET_IDLE:       return "IDLE";
    case EPI_DET_SUSPECT:    return "SUSPECT";
    case EPI_DET_CONFIRMING: return "CONFIRMING";
    case EPI_DET_ALARM:      return "ALARM";
    case EPI_DET_REJECTED:   return "REJECTED";
    default:                 return "?";
    }
}
