/*
 * epi_logic — logika urządzenia w wersji na Arduino Nano (ATmega328P).
 *
 * Odpowiednik firmware/lib/ przepisany pod 2 kB pamięci RAM: okno analizy
 * trzyma próbki jako liczby całkowite i zakłada stałą częstotliwość
 * próbkowania zamiast znaczników czasu. Progi, priorytety diody i tablica
 * napięć ogniwa są takie same jak w wersji na nRF54L15; pilnuje tego test
 * tests/test_nano.c, który porównuje obie implementacje.
 *
 * Plik nie wywołuje żadnej funkcji Arduino, więc kompiluje się też na
 * komputerze.
 */
#ifndef EPI_LOGIC_H
#define EPI_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

/* Szkic Arduino kompiluje się jako C++, a ten plik jako C, więc deklaracje
 * muszą wyjść z konsolidacją C — inaczej nazwy się nie zgadzają. */
#ifdef __cplusplus
extern "C" {
#endif

/* Próbkowanie akcelerometru. 25 Hz wystarcza na pasmo do 5,5 Hz z zapasem,
 * a okno 4 s mieści się w 100 próbkach. */
#define EPI_FS_HZ        25u
#define EPI_WINDOW_S     4u
#define EPI_N_CAP        128u   /* 128 * 2 B = 256 B pamięci RAM */
#define EPI_HR_CAP       16u

#define EPI_TICK_MS      250u   /* okres wywołania epi_tick() */
#define EPI_SOC_RED      10u
#define EPI_BOOT_MS      2500u
#define EPI_BATT_CUTOFF_MV 3400u

typedef enum {
    EPI_OFF = 0,
    EPI_IDLE,
    EPI_SUSPECT,
    EPI_CONFIRMING,
    EPI_ALARM,
    EPI_REJECTED
} epi_state;

typedef enum {
    EPI_LED_ALARM = 0,
    EPI_LED_CONFIRMING,
    EPI_LED_CHARGED,
    EPI_LED_CHARGING,
    EPI_LED_FAULT,
    EPI_LED_BOOT,
    EPI_LED_LOW_BATTERY,
    EPI_LED_MONITOR,
    EPI_LED_OFF
} epi_led_mode;

typedef enum {
    EPI_BTN_NONE = 0,
    EPI_BTN_CLICK,
    EPI_BTN_LONG,
    EPI_BTN_RESET
} epi_btn_event;

typedef struct {
    float amp_min;
    float freq_min;
    float freq_max;
    float hold_s;
    float window_s;
    float hr_rise_pct;
    float hr_slope_bpm_s;
    float spo2_drop_pp;
    bool  require_both;   /* tętno i SpO2 naraz zamiast alternatywy */
    bool  require_bio;    /* false = tryb pokazowy, sam ruch wystarcza */
    float cooldown_s;
} epi_cfg;

typedef struct {
    float amp;
    float freq;
    float hr_slope;
    bool  motion_pass;
    bool  hr_pass;
    bool  spo2_pass;
} epi_analysis;

typedef struct {
    epi_cfg cfg;
    epi_state state;
    epi_analysis an;

    /* Moduł przyspieszenia w setnych m/s^2; zakres do 327 m/s^2. */
    int16_t  mag[EPI_N_CAP];
    uint8_t  head;
    uint8_t  len;

    int16_t  hr_hist[EPI_HR_CAP];  /* tętno w bpm */
    uint32_t hr_t[EPI_HR_CAP];     /* znacznik czasu próbki tętna */
    uint8_t  hr_head;
    uint8_t  hr_len;

    float baseline_hr;
    float baseline_spo2;
    float hr;
    float spo2;

    uint32_t pattern_since_ms;
    uint32_t pattern_lost_ms;
    uint32_t confirm_until_ms;
    uint32_t cooldown_until_ms;
    uint32_t boot_ms;

    /* Przycisk */
    bool     btn_stable;
    bool     btn_raw_last;
    uint32_t btn_raw_since_ms;
    uint32_t btn_press_start_ms;
    bool     btn_long_sent;
    bool     btn_reset_sent;

    /* Bateria i zasilanie */
    uint16_t mv_filtered;
    uint8_t  soc;
    bool     batt_primed;
    bool     charger_present;
    bool     charge_complete;
    bool     powered_on;
    bool     sos_active;
    bool     fault;
    uint8_t  brightness;
} epi_ctx;

void  epi_cfg_defaults(epi_cfg *cfg);
void  epi_init(epi_ctx *c, const epi_cfg *cfg, uint32_t now_ms);

void  epi_push_accel(epi_ctx *c, float mag_ms2);
void  epi_push_vitals(epi_ctx *c, uint32_t now_ms, float hr, float spo2);
epi_state epi_tick(epi_ctx *c, uint32_t now_ms);
void  epi_cancel(epi_ctx *c, uint32_t now_ms);
void  epi_features(const epi_ctx *c, float *amp, float *freq);

epi_btn_event epi_btn_poll(epi_ctx *c, bool pressed, uint32_t now_ms);

uint8_t epi_soc_from_mv(uint16_t mv);
uint8_t epi_batt_tick(epi_ctx *c, uint16_t mv);

epi_led_mode epi_led_mode_of(const epi_ctx *c, uint32_t now_ms);
/* Wypełnienia PWM 0..255 dla kanałów R, G, B w chwili now_ms. */
void  epi_led_rgb(const epi_ctx *c, uint32_t now_ms, uint8_t out[3]);
void  epi_soc_color(uint8_t soc, uint8_t out[3]);

const char *epi_state_name(epi_state s);

#ifdef __cplusplus
}
#endif

#endif /* EPI_LOGIC_H */
