#include "epi_battery.h"

/* Napięcie spoczynkowe ogniwa Li-Pol 1S co 10 % pojemności, od 100 % w dół.
 * Wartości orientacyjne, do zastąpienia pomiarem. */
static const uint16_t ocv_mv[11] = {
    4200, 4060, 3980, 3920, 3870, 3820, 3790, 3770, 3740, 3680, 3300
};

uint8_t epi_batt_soc_from_mv(uint16_t mv)
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

void epi_batt_init(epi_batt *b)
{
    b->mv_filtered = 0;
    b->soc = 0;
    b->primed = false;
}

uint8_t epi_batt_update(epi_batt *b, uint16_t mv, bool charging)
{
    uint8_t soc;

    if (!b->primed) {
        b->mv_filtered = mv;
        b->primed = true;
        b->soc = epi_batt_soc_from_mv(mv);
        return b->soc;
    }

    /* EMA o współczynniku 1/8 — przy pomiarze raz na 30 s stała czasowa
     * wychodzi kilka minut, czyli dłużej niż każdy impuls obciążenia. */
    b->mv_filtered = (uint16_t)(((uint32_t)b->mv_filtered * 7u + mv) / 8u);
    soc = epi_batt_soc_from_mv(b->mv_filtered);

    if (charging) {
        if (soc > b->soc) {
            b->soc = soc;
        }
    } else if (soc < b->soc) {
        b->soc = soc;
    }
    return b->soc;
}

uint16_t epi_batt_mv_from_adc(uint16_t raw, uint16_t adc_max, uint16_t vref_mv)
{
    uint32_t at_pin;

    if (adc_max == 0u) {
        return 0u;
    }
    at_pin = ((uint32_t)raw * (uint32_t)vref_mv) / (uint32_t)adc_max;
    return (uint16_t)(at_pin * 2u); /* dzielnik 1:2 */
}
