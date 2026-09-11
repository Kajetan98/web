/*
 * Porównanie wersji na Arduino Nano (arduino/EpiNano/epi_logic.c) z wersją
 * na nRF54L15 (lib/). Obie mają rozstrzygać tak samo; test pilnuje, żeby nie
 * rozjechały się przy zmianach w jednej z nich. Implementacja z lib/ jest
 * widoczna przez nano_ref.h, bo nagłówki obu wersji używają tych samych nazw.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "epi_logic.h"
#include "nano_ref.h"

static int failures;
static int checks;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        checks++;                                           \
        if (!(cond)) {                                      \
            failures++;                                     \
            printf("FAIL %s:%d  ", __FILE__, __LINE__);     \
            printf(__VA_ARGS__);                            \
            printf("\n");                                   \
        }                                                   \
    } while (0)

static void test_colors_match(void)
{
    int soc;

    for (soc = 0; soc <= 100; soc++) {
        uint8_t nano[3], ref[3];
        epi_soc_color((uint8_t)soc, nano);
        ref_soc_color((uint8_t)soc, ref);
        CHECK(!memcmp(nano, ref, 3),
              "barwa dla soc=%d: nano %u/%u/%u, nRF %u/%u/%u", soc,
              nano[0], nano[1], nano[2], ref[0], ref[1], ref[2]);
    }
}

static void test_battery_match(void)
{
    int mv;

    for (mv = 3200; mv <= 4300; mv += 5) {
        uint8_t a = epi_soc_from_mv((uint16_t)mv);
        uint8_t b = ref_soc_from_mv((uint16_t)mv);
        CHECK(a == b, "SoC przy %d mV: nano %u, nRF %u", mv, a, b);
    }
}

static void test_led_match(void)
{
    static const epi_state states[] = {EPI_IDLE, EPI_SUSPECT, EPI_CONFIRMING,
                                       EPI_ALARM};
    epi_ctx c;
    unsigned s;
    int chg, full, fault, sos, soc;
    uint32_t t;

    for (s = 0; s < sizeof(states) / sizeof(states[0]); s++) {
        for (chg = 0; chg < 2; chg++) {
            for (full = 0; full < 2; full++) {
                for (fault = 0; fault < 2; fault++) {
                    for (sos = 0; sos < 2; sos++) {
                        for (soc = 5; soc <= 100; soc += 19) {
                            epi_init(&c, NULL, 0);
                            c.state = states[s];
                            c.charger_present = chg;
                            c.charge_complete = full;
                            c.fault = fault;
                            c.sos_active = sos;
                            c.soc = (uint8_t)soc;
                            c.brightness = 60;

                            for (t = 0; t < 5200; t += 37) {
                                uint8_t nano[3], ref[3];
                                int mode_nano = (int)epi_led_mode_of(&c, t);
                                int mode_ref = ref_led_mode(1, chg, full, fault,
                                                            sos, (uint8_t)soc,
                                                            (int)c.state, t);
                                epi_led_rgb(&c, t, nano);
                                ref_led_render(1, chg, full, fault, sos,
                                               (uint8_t)soc, (int)c.state, t,
                                               60, t, ref);
                                CHECK(mode_nano == mode_ref,
                                      "tryb diody przy t=%u: nano %d, nRF %d",
                                      t, mode_nano, mode_ref);
                                CHECK(!memcmp(nano, ref, 3),
                                      "wypełnienie t=%u soc=%d: nano %u/%u/%u, "
                                      "nRF %u/%u/%u", t, soc, nano[0], nano[1],
                                      nano[2], ref[0], ref[1], ref[2]);
                            }
                        }
                    }
                }
            }
        }
    }
}

/* Wyłączone urządzenie bez ładowarki: obie wersje gaszą diodę. */
static void test_led_off_match(void)
{
    epi_ctx c;
    uint8_t nano[3], ref[3];

    epi_init(&c, NULL, 0);
    c.powered_on = false;
    c.state = EPI_OFF;
    epi_led_rgb(&c, 1234, nano);
    ref_led_render(0, 0, 0, 0, 0, c.soc, 0, 1234, 60, 1234, ref);
    CHECK(!memcmp(nano, ref, 3), "zgaszona dioda");
    CHECK(nano[0] == 0 && nano[1] == 0 && nano[2] == 0, "wszystkie kanały w zerze");
}

static void test_buttons_match(void)
{
    epi_ctx c;
    uint32_t t;

    epi_init(&c, NULL, 0);
    ref_button_init(0);

    for (t = 0; t <= 13000; t += 5) {
        int pressed = (t >= 100 && t < 300) ||     /* kliknięcie */
                      (t >= 500 && t < 515) ||     /* drganie styku */
                      (t >= 1000 && t < 3500) ||   /* przytrzymanie */
                      (t >= 4000);                 /* trzymanie do restartu */
        int a = (int)epi_btn_poll(&c, pressed != 0, t);
        int b = ref_button_update(pressed, t);
        CHECK(a == b, "zdarzenie przycisku przy t=%u: nano %d, nRF %d", t, a, b);
    }
}

static float shake(float t_s, float freq_hz, float peak)
{
    return 9.81f + peak * sinf(6.2831853f * freq_hz * t_s);
}

static void test_detector_match(void)
{
    epi_ctx c;
    uint32_t t;
    int diffs = 0, alarms_nano = 0, alarms_ref = 0;

    epi_init(&c, NULL, 0);
    ref_det_init();

    for (t = 0; t <= 25000; t += 40) {   /* 25 Hz, tempo wersji na Nano */
        float mag = shake((float)t / 1000.0f, 3.5f, 4.0f);
        epi_push_accel(&c, mag);
        ref_det_push_motion(t, mag);

        if (t % 1000u == 0u) {
            float hr = 70.0f + 3.5f * ((float)t / 1000.0f);
            epi_push_vitals(&c, t, hr, 97.0f);
            ref_det_push_vitals(t, hr, 97.0f);
        }
        if (t % 240u == 0u) {
            float amp_ref, freq_ref;
            int a = (int)epi_tick(&c, t);
            int b = ref_det_tick(t, &amp_ref, &freq_ref);

            if (a != b) {
                diffs++;
            }
            if (a == (int)EPI_ALARM) {
                alarms_nano++;
            }
            if (b == (int)EPI_ALARM) {
                alarms_ref++;
            }
            if (a == (int)EPI_SUSPECT || a == (int)EPI_CONFIRMING) {
                CHECK(fabsf(c.an.amp - amp_ref) < 0.05f,
                      "amplituda t=%u: nano %.3f, nRF %.3f", t, c.an.amp, amp_ref);
                CHECK(fabsf(c.an.freq - freq_ref) < 0.05f,
                      "częstotliwość t=%u: nano %.3f, nRF %.3f", t, c.an.freq,
                      freq_ref);
            }
        }
    }
    CHECK(diffs == 0, "stany rozjechały się w %d chwilach", diffs);
    CHECK(alarms_nano > 0 && alarms_ref > 0,
          "obie wersje podniosły alarm (nano %d, nRF %d)", alarms_nano, alarms_ref);
}

/* Tryb pokazowy bez pulsoksymetru: alarm z samego kryterium ruchowego. */
static void test_demo_mode(void)
{
    epi_ctx c;
    epi_cfg cfg;
    uint32_t t;
    bool alarm = false;

    epi_cfg_defaults(&cfg);
    cfg.require_bio = false;
    epi_init(&c, &cfg, 0);

    for (t = 0; t <= 25000; t += 40) {
        epi_push_accel(&c, shake((float)t / 1000.0f, 3.5f, 4.0f));
        if (t % 240u == 0u && epi_tick(&c, t) == EPI_ALARM) {
            alarm = true;
            break;
        }
    }
    CHECK(alarm, "w trybie pokazowym sam ruch podnosi alarm");
    CHECK(t >= 18000u && t <= 20000u, "alarm po 3 s i 15 s, jest %u ms", t);
}

/* Bez kryterium biometrycznego i z wymaganym torem PPG alarmu nie ma. */
static void test_motion_only_rejected(void)
{
    epi_ctx c;
    uint32_t t;
    bool alarm = false;

    epi_init(&c, NULL, 0);   /* require_bio = true */
    for (t = 0; t <= 25000; t += 40) {
        epi_push_accel(&c, shake((float)t / 1000.0f, 3.5f, 4.0f));
        if (t % 1000u == 0u) {
            epi_push_vitals(&c, t, 72.0f, 97.0f);
        }
        if (t % 240u == 0u && epi_tick(&c, t) == EPI_ALARM) {
            alarm = true;
            break;
        }
    }
    CHECK(!alarm, "sam ruch nie wystarcza, gdy wymagane jest kryterium biometryczne");
}

/* Zużycie pamięci kontekstu — na ATmega328P jest jej 2 kB. */
static void test_footprint(void)
{
    printf("sizeof(epi_ctx) = %u B\n", (unsigned)sizeof(epi_ctx));
    CHECK(sizeof(epi_ctx) < 900u,
          "kontekst zajmuje %u B, zostaje za mało na bufory i stos",
          (unsigned)sizeof(epi_ctx));
}

int main(void)
{
    test_colors_match();
    test_battery_match();
    test_led_match();
    test_led_off_match();
    test_buttons_match();
    test_detector_match();
    test_demo_mode();
    test_motion_only_rejected();
    test_footprint();

    printf("%d sprawdzeń, %d niepowodzeń\n", checks, failures);
    return failures ? 1 : 0;
}
