#include "ble_epi.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_epi, CONFIG_LOG_DEFAULT_LEVEL);

/* --- Pulse Oximeter Service (0x1822) ---------------------------------- */
/* Zephyr nie ma gotowej implementacji PLXS, więc charakterystyka pomiaru
 * ciągłego (0x2A5F) jest zdefiniowana tutaj. Format zgodny z tym, co dekoduje
 * aplikacja: bajt flag, potem SpO2 i tętno jako SFLOAT (IEEE 11073). */

#define BT_UUID_PLXS       BT_UUID_DECLARE_16(0x1822)
#define BT_UUID_PLX_CONT   BT_UUID_DECLARE_16(0x2A5F)

static uint8_t plx_ccc_state;

static void plx_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    plx_ccc_state = (value == BT_GATT_CCC_NOTIFY) ? 1u : 0u;
}

BT_GATT_SERVICE_DEFINE(plxs_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_PLXS),
    BT_GATT_CHARACTERISTIC(BT_UUID_PLX_CONT, BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(plx_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* Zapis liczby zmiennoprzecinkowej w formacie SFLOAT: 4 bity wykładnika,
 * 12 bitów mantysy. Mantysa mieści się w zakresie -2048..2047, więc dla
 * wartości powyżej 204,7 wykładnik przechodzi z -1 na 0 (pełne jednostki). */
static void put_sfloat(uint8_t *dst, float value)
{
    int32_t mantissa = (int32_t)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
    uint16_t exponent = 0x0Fu; /* -1 */
    uint16_t raw;

    if (mantissa > 2047 || mantissa < -2048) {
        mantissa = (int32_t)(value + (value >= 0.0f ? 0.5f : -0.5f));
        exponent = 0x00u;
    }
    if (mantissa > 2047) {
        mantissa = 2047;
    }
    if (mantissa < -2048) {
        mantissa = -2048;
    }
    raw = (uint16_t)((exponent << 12) | ((uint16_t)mantissa & 0x0FFFu));
    dst[0] = (uint8_t)(raw & 0xFFu);
    dst[1] = (uint8_t)(raw >> 8);
}

void ble_epi_notify_spo2(float spo2, float pulse_rate)
{
    uint8_t buf[5];

    if (!plx_ccc_state) {
        return;
    }
    buf[0] = 0x00; /* brak pól opcjonalnych */
    put_sfloat(&buf[1], spo2);
    put_sfloat(&buf[3], pulse_rate);
    (void)bt_gatt_notify(NULL, &plxs_svc.attrs[1], buf, sizeof(buf));
}

/* --- usługa własna EPI ------------------------------------------------- */
/* Losowy identyfikator bazowy; zmiana wymaga zmiany także po stronie aplikacji.
 * 6e5a0001-b5a3-f393-e0a9-e50e24dcca9e */
#define BT_UUID_EPI_SVC_VAL \
    BT_UUID_128_ENCODE(0x6e5a0001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define BT_UUID_EPI_STATE_VAL \
    BT_UUID_128_ENCODE(0x6e5a0002, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)

static struct bt_uuid_128 uuid_epi_svc = BT_UUID_INIT_128(BT_UUID_EPI_SVC_VAL);
static struct bt_uuid_128 uuid_epi_state = BT_UUID_INIT_128(BT_UUID_EPI_STATE_VAL);

static uint8_t epi_ccc_state;

static void epi_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    epi_ccc_state = (value == BT_GATT_CCC_NOTIFY) ? 1u : 0u;
}

BT_GATT_SERVICE_DEFINE(epi_svc,
    BT_GATT_PRIMARY_SERVICE(&uuid_epi_svc),
    BT_GATT_CHARACTERISTIC(&uuid_epi_state.uuid, BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(epi_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* Ramka stanu: 1 B stan, 1 B flagi kryteriów, 2 B amplituda (setne m/s^2),
 * 2 B częstotliwość (setne Hz), 2 B nachylenie tętna (setne bpm/s). */
void ble_epi_notify_state(epi_det_state state, const epi_det_analysis *an)
{
    uint8_t buf[8];
    uint16_t amp = (uint16_t)(an->amp * 100.0f);
    uint16_t freq = (uint16_t)(an->freq * 100.0f);
    int16_t slope = (int16_t)(an->hr_slope * 100.0f);

    if (!epi_ccc_state) {
        return;
    }
    buf[0] = (uint8_t)state;
    buf[1] = (uint8_t)((an->motion_pass ? 1u : 0u) |
                       (an->hr_pass ? 2u : 0u) |
                       (an->spo2_pass ? 4u : 0u));
    buf[2] = (uint8_t)(amp & 0xFFu);
    buf[3] = (uint8_t)(amp >> 8);
    buf[4] = (uint8_t)(freq & 0xFFu);
    buf[5] = (uint8_t)(freq >> 8);
    buf[6] = (uint8_t)((uint16_t)slope & 0xFFu);
    buf[7] = (uint8_t)((uint16_t)slope >> 8);
    (void)bt_gatt_notify(NULL, &epi_svc.attrs[1], buf, sizeof(buf));
}

void ble_epi_notify_hr(uint8_t bpm)
{
    (void)bt_hrs_notify(bpm);
}

void ble_epi_notify_battery(uint8_t soc)
{
    (void)bt_bas_set_battery_level(soc);
}

/* --- rozgłaszanie ------------------------------------------------------ */

static const struct bt_data adv[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    /* Aplikacja filtruje po tych dwóch usługach, więc muszą być w rozgłoszeniu. */
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x0D, 0x18, 0x22, 0x18, 0x0F, 0x18),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

int ble_epi_init(void)
{
    int err = bt_enable(NULL);

    if (err) {
        LOG_ERR("bt_enable: %d", err);
        return err;
    }
    /* Nazwa makra parametrów rozgłaszania zależy od wersji Zephyra
     * (BT_LE_ADV_CONN_FAST_2 w nowszych, BT_LE_ADV_CONN w starszych). */
    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, adv, ARRAY_SIZE(adv), NULL, 0);
    if (err) {
        LOG_ERR("adv_start: %d", err);
    }
    return err;
}
