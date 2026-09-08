/*
 * epi_detector — port automatu detekcji z aplikacji (aplikacja/app.js) na C.
 *
 * Ten sam podział na stany, te same cechy sygnału i te same progi domyślne,
 * żeby urządzenie i aplikacja rozstrzygały tak samo. Etap pierwszy modelu:
 * reguła deterministyczna, bez uczenia. Wyliczone cechy (amp, freq, hr_slope)
 * są jednocześnie wejściem dla klasyfikatora z etapu drugiego.
 */
#ifndef EPI_DETECTOR_H
#define EPI_DETECTOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EPI_DET_OFF = 0,
    EPI_DET_IDLE,
    EPI_DET_SUSPECT,
    EPI_DET_CONFIRMING,
    EPI_DET_ALARM,
    EPI_DET_REJECTED,
} epi_det_state;

/* Progi. Wartości domyślne pochodzą z aplikacji i wymagają kalibracji na
 * danych rzeczywistych — patrz hardware/README.md, rozdział o modelu. */
typedef struct {
    float    amp_min;       /* m/s^2, RMS przyspieszenia bez składowej stałej */
    float    freq_min;      /* Hz, dolna granica pasma napadowego */
    float    freq_max;      /* Hz */
    float    hold_s;        /* ile sekund wzorzec musi się utrzymać */
    float    window_s;      /* długość okna potwierdzenia */
    float    hr_rise_pct;   /* wzrost tętna ponad linię bazową */
    float    hr_slope_bpm_s;/* tempo narastania tętna */
    float    spo2_drop_pp;  /* spadek SpO2 w punktach procentowych */
    bool     require_both;  /* true: tętno i SpO2 naraz; false: alternatywa */
    float    cooldown_s;    /* wyciszenie po rozstrzygnięciu */
} epi_det_cfg;

typedef struct {
    float amp;
    float freq;
    float hr_slope;
    bool  motion_pass;
    bool  hr_pass;
    bool  spo2_pass;
} epi_det_analysis;

#define EPI_MOTION_CAP 384u /* 6 s przy 64 Hz */
#define EPI_HR_CAP      32u /* 32 s przy 1 Hz */

typedef struct {
    uint32_t t_ms;
    float    mag;
} epi_motion_sample;

typedef struct {
    uint32_t t_ms;
    float    hr;
} epi_hr_sample;

typedef struct {
    epi_det_cfg cfg;
    epi_det_state state;
    epi_det_analysis an;

    float baseline_hr;
    float baseline_spo2;
    float hr;
    float spo2;

    epi_motion_sample motion[EPI_MOTION_CAP];
    uint16_t motion_head;
    uint16_t motion_len;

    epi_hr_sample hr_hist[EPI_HR_CAP];
    uint8_t hr_head;
    uint8_t hr_len;

    uint32_t pattern_since_ms;
    uint32_t pattern_lost_ms;
    uint32_t confirm_until_ms;
    uint32_t cooldown_until_ms;
    uint32_t state_since_ms;
} epi_detector;

void epi_det_defaults(epi_det_cfg *cfg);
void epi_det_init(epi_detector *d, const epi_det_cfg *cfg, uint32_t now_ms);

/* Moduł wektora przyspieszenia w m/s^2, razem z grawitacją. */
void epi_det_push_motion(epi_detector *d, uint32_t t_ms, float mag);
void epi_det_push_vitals(epi_detector *d, uint32_t t_ms, float hr, float spo2);

/* Wywoływane cyklicznie (w aplikacji co 250 ms). Zwraca stan po aktualizacji. */
epi_det_state epi_det_tick(epi_detector *d, uint32_t now_ms);

/* Anulowanie przyciskiem w oknie potwierdzenia albo skasowanie alarmu. */
void epi_det_cancel(epi_detector *d, uint32_t now_ms);

/* Cechy z okna analizy — wystawione osobno, bo są też wejściem klasyfikatora. */
void epi_det_features(const epi_detector *d, uint32_t now_ms,
                      float window_s, float *amp, float *freq);

const char *epi_det_state_name(epi_det_state s);

#endif /* EPI_DETECTOR_H */
