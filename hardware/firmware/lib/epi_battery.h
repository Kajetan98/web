/*
 * epi_battery — napięcie ogniwa Li-Pol 1S na stan naładowania.
 *
 * Tablica napięć jest przybliżeniem krzywej rozładowania przy małym prądzie
 * i służy jako punkt wyjścia. Docelowo trzeba ją zastąpić krzywą zmierzoną na
 * egzemplarzu ogniwa użytego w urządzeniu (procedura w README).
 */
#ifndef EPI_BATTERY_H
#define EPI_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#define EPI_BATT_FULL_MV   4200u
#define EPI_BATT_EMPTY_MV  3300u
/* Poniżej tego napięcia firmware wyłącza urządzenie, żeby nie zejść do
 * napięcia, przy którym ogniwo traci pojemność. */
#define EPI_BATT_CUTOFF_MV 3400u

typedef struct {
    uint16_t mv_filtered; /* wygładzone napięcie */
    uint8_t  soc;         /* ostatnio zaraportowany SoC */
    bool     primed;
} epi_batt;

/* Interpolacja z tablicy OCV. Bez filtrowania. */
uint8_t epi_batt_soc_from_mv(uint16_t mv);

void epi_batt_init(epi_batt *b);

/* Filtr wykładniczy plus histereza: SoC rośnie tylko przy ładowaniu, więc
 * wskazanie nie skacze w górę po zdjęciu obciążenia (błysk diody, wibracja). */
uint8_t epi_batt_update(epi_batt *b, uint16_t mv, bool charging);

/* Przeliczenie odczytu ADC na napięcie ogniwa dla dzielnika 1:2. */
uint16_t epi_batt_mv_from_adc(uint16_t raw, uint16_t adc_max, uint16_t vref_mv);

#endif /* EPI_BATTERY_H */
