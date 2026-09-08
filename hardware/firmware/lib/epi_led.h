/*
 * epi_led — sterowanie diodą RGB: priorytety stanów, mapowanie stanu
 * naładowania na kolor, wzorce błysków.
 *
 * Moduł nie ma zależności poza stdint/stdbool, żeby dało się go testować
 * na komputerze i przenieść na inną płytkę razem z resztą logiki.
 */
#ifndef EPI_LED_H
#define EPI_LED_H

#include <stdbool.h>
#include <stdint.h>

#include "epi_detector.h"

/* Tryby świecenia. Kolejność w enumie odpowiada priorytetom (0 = najwyższy). */
typedef enum {
    EPI_LED_ALARM = 0,   /* napad potwierdzony albo ręczne SOS */
    EPI_LED_CONFIRMING,  /* okno potwierdzenia, alarm jeszcze do anulowania */
    EPI_LED_CHARGED,     /* na ładowarce, ogniwo pełne */
    EPI_LED_CHARGING,    /* na ładowarce, ładowanie w toku */
    EPI_LED_FAULT,       /* czujnik nie odpowiada, brak kalibracji, błąd zapisu */
    EPI_LED_BOOT,        /* pierwsze sekundy po włączeniu: pokaz poziomu baterii */
    EPI_LED_LOW_BATTERY, /* SoC <= EPI_LED_SOC_RED */
    EPI_LED_MONITOR,     /* praca normalna */
    EPI_LED_OFF,         /* urządzenie wyłączone */
} epi_led_mode;

/* Poniżej tego SoC odcień jest już czysto czerwony i dochodzi drugi błysk. */
#define EPI_LED_SOC_RED    10u
/* Czas pokazywania poziomu baterii po włączeniu. */
#define EPI_LED_BOOT_MS    2500u

typedef struct {
    uint8_t r, g, b;
} epi_rgb;

typedef struct {
    bool     powered_on;      /* urządzenie włączone długim przytrzymaniem */
    bool     charger_present; /* wykryte napięcie z ładowarki indukcyjnej */
    bool     charge_complete; /* wyjście STAT układu ładowania */
    bool     fault;
    bool     sos_active;      /* SOS wywołane przyciskiem */
    uint8_t  soc;             /* 0..100 % */
    epi_det_state detector;
    uint32_t uptime_ms;       /* czas od włączenia, do trybu BOOT */
    uint8_t  brightness;      /* jasność główna 0..255 */
} epi_led_input;

typedef struct {
    uint16_t period_ms; /* 0 = świecenie ciągłe */
    uint16_t on_ms;     /* długość pojedynczego błysku */
    uint16_t gap_ms;    /* odstęp między błyskami w serii */
    uint8_t  pulses;    /* liczba błysków w okresie */
    bool     breathe;   /* płynne narastanie i gaśnięcie zamiast błysku */
} epi_led_pattern;

/* Rozstrzyga, który tryb obowiązuje przy danym stanie urządzenia. */
epi_led_mode epi_led_mode_for(const epi_led_input *in);

/* Kolor bazowy trybu; dla MONITOR i BOOT zależy od poziomu naładowania. */
epi_rgb epi_led_color_for(epi_led_mode mode, uint8_t soc);

/* Zielony (100%) -> żółty -> czerwony (EPI_LED_SOC_RED i niżej). */
epi_rgb epi_led_soc_color(uint8_t soc);

epi_led_pattern epi_led_pattern_for(epi_led_mode mode);

/* Wypełnienia PWM na trzy kanały dla chwili now_ms. */
epi_rgb epi_led_render(const epi_led_input *in, uint32_t now_ms);

/* Korekcja gamma dla obwiedni jasności (płynne przejścia). */
uint8_t epi_gamma8(uint8_t v);

/* Zalecany okres odświeżania w danym trybie; w OFF i CHARGED nie ma potrzeby
 * budzić procesora częściej niż raz na sekundę. */
uint16_t epi_led_tick_ms(epi_led_mode mode);

#endif /* EPI_LED_H */
