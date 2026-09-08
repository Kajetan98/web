/*
 * Warstwa BLE. Aplikacja (aplikacja/app.js) filtruje urządzenia po usługach
 * Heart Rate (0x180D) i Pulse Oximeter (0x1822), a opcjonalnie czyta Battery
 * Service (0x180F). Te trzy usługi są więc obowiązkowe.
 *
 * Usługa własna EPI przenosi to, czego nie da się wyrazić profilami
 * standardowymi: stan automatu, zdarzenie alarmu, cechy sygnału ruchowego,
 * progi i synchronizację czasu.
 */
#ifndef BLE_EPI_H
#define BLE_EPI_H

#include <stdint.h>

#include "epi_detector.h"

int  ble_epi_init(void);
void ble_epi_notify_hr(uint8_t bpm);
void ble_epi_notify_spo2(float spo2, float pulse_rate);
void ble_epi_notify_battery(uint8_t soc);
void ble_epi_notify_state(epi_det_state state, const epi_det_analysis *an);

#endif /* BLE_EPI_H */
