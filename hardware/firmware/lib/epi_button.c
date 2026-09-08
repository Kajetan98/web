#include "epi_button.h"

void epi_button_init(epi_button *b, uint32_t now_ms)
{
    b->stable = false;
    b->raw_last = false;
    b->raw_since_ms = now_ms;
    b->press_start_ms = 0;
    b->long_sent = false;
    b->reset_sent = false;
    b->primed = true;
}

epi_btn_event epi_button_update(epi_button *b, bool raw_pressed, uint32_t now_ms)
{
    epi_btn_event evt = EPI_BTN_EVT_NONE;

    if (raw_pressed != b->raw_last) {
        b->raw_last = raw_pressed;
        b->raw_since_ms = now_ms;
    }

    /* Zmiana stanu liczy się dopiero, gdy utrzyma się przez czas filtrowania. */
    if (raw_pressed != b->stable &&
        (now_ms - b->raw_since_ms) >= EPI_BTN_DEBOUNCE_MS) {
        b->stable = raw_pressed;
        if (raw_pressed) {
            b->press_start_ms = b->raw_since_ms;
            b->long_sent = false;
            b->reset_sent = false;
        } else if (!b->long_sent && !b->reset_sent) {
            uint32_t held = b->raw_since_ms - b->press_start_ms;
            if (held <= EPI_BTN_CLICK_MAX_MS) {
                evt = EPI_BTN_EVT_CLICK;
            }
        }
        return evt;
    }

    if (b->stable) {
        uint32_t held = now_ms - b->press_start_ms;
        if (!b->reset_sent && held >= EPI_BTN_RESET_MS) {
            b->reset_sent = true;
            return EPI_BTN_EVT_RESET;
        }
        if (!b->long_sent && held >= EPI_BTN_LONG_MS) {
            b->long_sent = true;
            return EPI_BTN_EVT_LONG;
        }
    }
    return EPI_BTN_EVT_NONE;
}
