/*
 * epi_button — dekoder gestów przycisku tact switch.
 *
 * Krótkie kliknięcie: SOS, a w trakcie alarmu jego anulowanie.
 * Przytrzymanie: włączenie albo wyłączenie urządzenia.
 * Bardzo długie przytrzymanie: twardy restart (wyjście z zawieszenia).
 */
#ifndef EPI_BUTTON_H
#define EPI_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#define EPI_BTN_DEBOUNCE_MS   30u
#define EPI_BTN_CLICK_MAX_MS  700u
#define EPI_BTN_LONG_MS      2000u
#define EPI_BTN_RESET_MS     8000u

typedef enum {
    EPI_BTN_EVT_NONE = 0,
    EPI_BTN_EVT_CLICK,   /* zwolniony przed EPI_BTN_CLICK_MAX_MS */
    EPI_BTN_EVT_LONG,    /* zgłaszany w trakcie trzymania, raz */
    EPI_BTN_EVT_RESET,   /* zgłaszany w trakcie trzymania, raz */
} epi_btn_event;

typedef struct {
    bool     stable;        /* stan po odfiltrowaniu drgań styku */
    bool     raw_last;
    uint32_t raw_since_ms;
    uint32_t press_start_ms;
    bool     long_sent;
    bool     reset_sent;
    bool     primed;
} epi_button;

void epi_button_init(epi_button *b, uint32_t now_ms);

/* raw_pressed: stan wejścia po zamianie polaryzacji (true = wciśnięty). */
epi_btn_event epi_button_update(epi_button *b, bool raw_pressed, uint32_t now_ms);

#endif /* EPI_BUTTON_H */
