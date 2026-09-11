/*
 * Cienka warstwa nad firmware/lib/ na potrzeby testu porównawczego.
 * Nagłówki lib/ i arduino/EpiNano/epi_logic.h mają te same nazwy typów
 * i stałych, więc nie dają się włączyć do jednej jednostki kompilacji.
 * Ten plik odsłania funkcje lib/ pod innymi nazwami i na prostych typach.
 */
#ifndef NANO_REF_H
#define NANO_REF_H

#include <stdint.h>

void    ref_soc_color(uint8_t soc, uint8_t out[3]);
uint8_t ref_soc_from_mv(uint16_t mv);

/* det_state i zwracany tryb diody używają tej samej numeracji co epi_logic.h. */
int  ref_led_mode(int powered, int charger, int complete, int fault, int sos,
                  uint8_t soc, int det_state, uint32_t uptime_ms);
void ref_led_render(int powered, int charger, int complete, int fault, int sos,
                    uint8_t soc, int det_state, uint32_t uptime_ms,
                    uint8_t brightness, uint32_t now_ms, uint8_t out[3]);

void ref_button_init(uint32_t now_ms);
int  ref_button_update(int pressed, uint32_t now_ms);

void ref_det_init(void);
void ref_det_push_motion(uint32_t t_ms, float mag);
void ref_det_push_vitals(uint32_t t_ms, float hr, float spo2);
int  ref_det_tick(uint32_t t_ms, float *amp, float *freq);

#endif /* NANO_REF_H */
