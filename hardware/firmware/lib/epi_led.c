#include "epi_led.h"

/* Kalibracja kanałów. Zielona i niebieska struktura w typowej diodzie RGB mają
 * wyższą skuteczność świetlną niż czerwona, więc przy równym wypełnieniu
 * mieszanka wychodzi zielonkawa. Wartości do zmierzenia na docelowej diodzie
 * i rezystorach — patrz README, rozdział o diodzie. */
#ifndef EPI_LED_GAIN_R
#define EPI_LED_GAIN_R 255
#endif
#ifndef EPI_LED_GAIN_G
#define EPI_LED_GAIN_G 115
#endif
#ifndef EPI_LED_GAIN_B
#define EPI_LED_GAIN_B 140
#endif

static uint8_t scale8(uint8_t value, uint8_t factor)
{
    return (uint8_t)(((uint16_t)value * (uint16_t)factor + 127u) / 255u);
}

/* HSV o pełnym nasyceniu i jasności, odcień w stopniach 0..360. */
static epi_rgb hue_rgb(uint16_t hue_deg)
{
    epi_rgb c = {0, 0, 0};
    uint16_t h = (uint16_t)(hue_deg % 360u);
    uint8_t sector = (uint8_t)(h / 60u);
    uint8_t frac = (uint8_t)(((uint32_t)(h % 60u) * 255u) / 60u);
    uint8_t rise = frac;
    uint8_t fall = (uint8_t)(255u - frac);

    switch (sector) {
    case 0: c.r = 255;  c.g = rise; c.b = 0;    break;
    case 1: c.r = fall; c.g = 255;  c.b = 0;    break;
    case 2: c.r = 0;    c.g = 255;  c.b = rise; break;
    case 3: c.r = 0;    c.g = fall; c.b = 255;  break;
    case 4: c.r = rise; c.g = 0;    c.b = 255;  break;
    default: c.r = 255; c.g = 0;    c.b = fall; break;
    }
    return c;
}

static epi_rgb apply_gain(epi_rgb c)
{
    c.r = scale8(c.r, EPI_LED_GAIN_R);
    c.g = scale8(c.g, EPI_LED_GAIN_G);
    c.b = scale8(c.b, EPI_LED_GAIN_B);
    return c;
}

epi_rgb epi_led_soc_color(uint8_t soc)
{
    uint16_t hue;

    if (soc > 100u) {
        soc = 100u;
    }
    if (soc <= EPI_LED_SOC_RED) {
        hue = 0u; /* czysta czerwień utrzymuje się aż do wyłączenia */
    } else {
        /* 100 % -> 120 stopni (zielony), EPI_LED_SOC_RED -> 0 (czerwony). */
        hue = (uint16_t)(((uint32_t)(soc - EPI_LED_SOC_RED) * 120u) /
                         (100u - EPI_LED_SOC_RED));
    }
    return apply_gain(hue_rgb(hue));
}

epi_rgb epi_led_color_for(epi_led_mode mode, uint8_t soc)
{
    epi_rgb off = {0, 0, 0};

    switch (mode) {
    case EPI_LED_ALARM:
    case EPI_LED_LOW_BATTERY:
        return apply_gain(hue_rgb(0));    /* czerwony */
    case EPI_LED_CONFIRMING:
        return apply_gain(hue_rgb(35));   /* bursztynowy */
    case EPI_LED_CHARGED:
        return apply_gain(hue_rgb(120));  /* zielony */
    case EPI_LED_CHARGING:
        return apply_gain(hue_rgb(225));  /* niebieski */
    case EPI_LED_FAULT:
        return apply_gain(hue_rgb(300));  /* magenta */
    case EPI_LED_MONITOR:
    case EPI_LED_BOOT:
        return epi_led_soc_color(soc);
    case EPI_LED_OFF:
    default:
        return off;
    }
}

epi_led_mode epi_led_mode_for(const epi_led_input *in)
{
    if (!in->powered_on && !in->charger_present) {
        return EPI_LED_OFF;
    }
    if (in->sos_active || in->detector == EPI_DET_ALARM) {
        return EPI_LED_ALARM;
    }
    if (in->detector == EPI_DET_CONFIRMING) {
        return EPI_LED_CONFIRMING;
    }
    if (in->charger_present) {
        return in->charge_complete ? EPI_LED_CHARGED : EPI_LED_CHARGING;
    }
    if (in->fault) {
        return EPI_LED_FAULT;
    }
    if (in->uptime_ms < EPI_LED_BOOT_MS) {
        return EPI_LED_BOOT;
    }
    if (in->soc <= EPI_LED_SOC_RED) {
        return EPI_LED_LOW_BATTERY;
    }
    return EPI_LED_MONITOR;
}

epi_led_pattern epi_led_pattern_for(epi_led_mode mode)
{
    epi_led_pattern p = {0, 0, 0, 1, false};

    switch (mode) {
    case EPI_LED_ALARM:       /* 4 Hz, wypełnienie 50 % */
        p.period_ms = 250; p.on_ms = 125; p.pulses = 1; break;
    case EPI_LED_CONFIRMING:  /* 2 Hz, krótki błysk */
        p.period_ms = 500; p.on_ms = 120; p.pulses = 1; break;
    case EPI_LED_CHARGED:     /* na ładowarce energia nie jest problemem */
        p.period_ms = 0; p.on_ms = 0; p.pulses = 1; break;
    case EPI_LED_CHARGING:
        p.period_ms = 3000; p.on_ms = 3000; p.pulses = 1; p.breathe = true; break;
    case EPI_LED_FAULT:
        p.period_ms = 5000; p.on_ms = 60; p.gap_ms = 200; p.pulses = 3; break;
    case EPI_LED_BOOT:
        p.period_ms = 0; p.on_ms = 0; p.pulses = 1; break;
    case EPI_LED_LOW_BATTERY: /* podwójny błysk odróżnia od alarmu */
        p.period_ms = 5000; p.on_ms = 40; p.gap_ms = 220; p.pulses = 2; break;
    case EPI_LED_MONITOR:
        p.period_ms = 5000; p.on_ms = 30; p.pulses = 1; break;
    case EPI_LED_OFF:
    default:
        p.period_ms = 1000; p.on_ms = 0; p.pulses = 0; break;
    }
    return p;
}

uint8_t epi_gamma8(uint8_t v)
{
    /* Przybliżenie gamma 2.2 na liczbach całkowitych: v^2 * v^0.2 pominięte,
     * zostaje v^2, co dla obwiedni oddechu wystarcza i nic nie kosztuje. */
    return (uint8_t)(((uint16_t)v * (uint16_t)v + 255u) / 256u);
}

/* Obwiednia jasności 0..255 dla danej fazy wzorca. */
static uint8_t envelope(const epi_led_pattern *p, uint32_t now_ms)
{
    uint32_t phase;
    uint32_t slot;
    uint8_t i;

    if (p->pulses == 0u) {
        return 0u;
    }
    if (p->period_ms == 0u) {
        return 255u; /* świecenie ciągłe */
    }

    phase = now_ms % p->period_ms;

    if (p->breathe) {
        uint32_t half = p->period_ms / 2u;
        uint32_t up = (phase < half) ? phase : (p->period_ms - phase);
        uint8_t lin = (uint8_t)((up * 255u) / (half ? half : 1u));
        return epi_gamma8(lin);
    }

    slot = (uint32_t)p->on_ms + (uint32_t)p->gap_ms;
    for (i = 0; i < p->pulses; i++) {
        uint32_t start = (uint32_t)i * slot;
        if (phase >= start && phase < start + (uint32_t)p->on_ms) {
            return 255u;
        }
    }
    return 0u;
}

epi_rgb epi_led_render(const epi_led_input *in, uint32_t now_ms)
{
    epi_led_mode mode = epi_led_mode_for(in);
    epi_led_pattern pat = epi_led_pattern_for(mode);
    epi_rgb c = epi_led_color_for(mode, in->soc);
    uint8_t env = envelope(&pat, now_ms);
    uint8_t level;

    if (env == 0u) {
        c.r = c.g = c.b = 0;
        return c;
    }
    /* Alarm zawsze świeci pełną jasnością, niezależnie od ustawienia. */
    level = (mode == EPI_LED_ALARM) ? 255u : in->brightness;
    level = scale8(level, env);

    c.r = scale8(c.r, level);
    c.g = scale8(c.g, level);
    c.b = scale8(c.b, level);
    return c;
}

uint16_t epi_led_tick_ms(epi_led_mode mode)
{
    switch (mode) {
    case EPI_LED_ALARM:
    case EPI_LED_CONFIRMING:
        return 10;
    case EPI_LED_CHARGING:
        return 25;  /* płynna obwiednia oddechu */
    case EPI_LED_FAULT:
    case EPI_LED_LOW_BATTERY:
    case EPI_LED_MONITOR:
        return 10;  /* tylko w oknie błysku, poza nim timer śpi do następnego */
    case EPI_LED_CHARGED:
    case EPI_LED_BOOT:
    case EPI_LED_OFF:
    default:
        return 1000;
    }
}
