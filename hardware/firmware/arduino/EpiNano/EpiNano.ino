/*
 * EPI — wersja na Arduino Nano (ATmega328P) z modułem Bluetooth HC-06.
 *
 * Ograniczenie, od którego trzeba zacząć: HC-06 to Bluetooth Classic
 * z profilem portu szeregowego (SPP), a aplikacja EPI w aplikacja/ łączy
 * się przez Web Bluetooth, czyli wyłącznie z urządzeniami BLE (GATT).
 * Ten układ nie połączy się z aplikacją. Rozmawia tekstem z dowolnym
 * terminalem szeregowym Bluetooth na Androidzie. Warianty, które dają
 * połączenie z aplikacją, opisuje hardware/firmware/arduino/README.md.
 *
 * Logika detekcji, diody, przycisku i baterii siedzi w epi_logic.c —
 * ten plik odpowiada tylko za wyprowadzenia, czasy i transmisję.
 */

#include <Wire.h>

#include "epi_logic.h"

/* --- opcje kompilacji -------------------------------------------------- */

/* 1: HC-06 na sprzętowym UART (D0/D1). Stabilne czasy, ale na czas wgrywania
 *    programu trzeba odłączyć przewód z D0.
 * 0: HC-06 na SoftwareSerial (D8/D12), USB zostaje wolne. SoftwareSerial
 *    wyłącza przerwania na czas nadawania bajtu, więc millis() gubi kilka
 *    milisekund na każdą wysłaną linię. */
#define EPI_BT_HARDWARE_SERIAL 1

/* 1: buzzer z generatorem (ten z listy części) — wystarczy podać napięcie.
 * 0: przetwornik bez generatora, sterowany funkcją tone(). Uwaga: tone()
 *    zajmuje Timer2, więc wyjścia PWM D3 i D11 przestają działać. */
#define EPI_BUZZER_ACTIVE 1

/* 1: pulsoksymetr MAX30102 podłączony i obsługiwany biblioteką DFRobot.
 * 0: brak pulsoksymetru — urządzenie pracuje w trybie pokazowym, w którym
 *    do alarmu wystarcza kryterium ruchowe. Na tym ustawieniu fałszywych
 *    alarmów jest znacznie więcej; do niczego poza demonstracją się nie nadaje. */
#define EPI_USE_PPG 0

#if EPI_USE_PPG
#include <DFRobot_BloodOxygen_S.h>
DFRobot_BloodOxygen_S_I2C ppg(&Wire, 0x57);
#endif

/* --- wyprowadzenia ----------------------------------------------------- */

const uint8_t PIN_BTN     = 2;   /* przycisk do masy, podciągnięcie wewnętrzne */
const uint8_t PIN_LED_R   = 9;   /* PWM, Timer1 */
const uint8_t PIN_LED_G   = 10;  /* PWM, Timer1 */
const uint8_t PIN_LED_B   = 6;   /* PWM, Timer0 */
const uint8_t PIN_BUZZER  = 4;   /* przez tranzystor */
const uint8_t PIN_MOTOR   = 7;   /* MOSFET + dioda gaszeniowa */
const uint8_t PIN_VBAT    = A0;  /* dzielnik 1:2 z ogniwa */
const uint8_t PIN_CHG     = A1;  /* wykrycie ładowarki, wejście cyfrowe */

/* Dioda ze wspólną anodą: 0 na wyjściu to pełna jasność. */
#define LED_COMMON_ANODE 1

#if EPI_BT_HARDWARE_SERIAL
#define BT Serial
#else
#include <SoftwareSerial.h>
const uint8_t PIN_BT_RX = 8;   /* do TXD modułu */
const uint8_t PIN_BT_TX = 12;  /* do RXD modułu przez dzielnik 1k/2k */
SoftwareSerial BT(PIN_BT_RX, PIN_BT_TX);
#endif

/* --- akcelerometr MPU-6050 --------------------------------------------- */
/* Adres 0x68 przy AD0 zwartym do masy (tak jest na module GY-521, kiedy pin
 * AD0 zostaje niepodłączony) albo 0x69 przy AD0 na zasilaniu. Rejestry należą
 * do wspólnego zestawu rodziny InvenSense, więc ten sam kod obsłuży też
 * ICG-20660L. Program wypisuje przy starcie zawartość WHO_AM_I: MPU-6050
 * zwraca 0x68, część tańszych zamienników 0x70, 0x72 albo 0x98. Odczyt 0x00
 * lub 0xFF oznacza, że czujnik nie odpowiada. */
#define IMU_ADDR        0x68
#define IMU_SMPLRT_DIV  0x19
#define IMU_CONFIG      0x1A
#define IMU_ACCEL_CONF  0x1C
#define IMU_ACCEL_XOUT  0x3B
#define IMU_PWR_MGMT_1  0x6B
#define IMU_WHO_AM_I    0x75
/* Zakres +-4 g: 8192 LSB na g. */
#define IMU_LSB_PER_G   8192.0f
#define G_MS2           9.80665f

/* --- czasy ------------------------------------------------------------- */

const uint16_t MS_SAMPLE    = 1000u / EPI_FS_HZ;  /* 40 ms */
const uint16_t MS_LED       = 10;
const uint16_t MS_BUTTON    = 10;
const uint16_t MS_TELEMETRY = 1000;
const uint16_t MS_VITALS    = 1000;
const uint32_t MS_BATTERY   = 30000;
const uint32_t MS_SOS_WINDOW = 10000;  /* czas na anulowanie SOS */

/* Napięcie odniesienia ADC w miliwoltach i dzielnik na wejściu baterii. */
const uint16_t ADC_VREF_MV = 5000;
const uint8_t  VBAT_DIV    = 2;

/* --- stan -------------------------------------------------------------- */

static epi_ctx ctx;
static uint32_t t_sample, t_led, t_button, t_telemetry, t_vitals, t_battery;
static uint32_t sos_deadline;
static bool sos_sent;
static uint32_t alarm_toggle_at;
static bool alarm_output;
static epi_state last_state;
static char line[40];
static uint8_t line_len;

/* --- czujnik ----------------------------------------------------------- */

static uint8_t imuRead8(uint8_t reg)
{
    Wire.beginTransmission(IMU_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return 0xFF;
    }
    if (Wire.requestFrom((uint8_t)IMU_ADDR, (uint8_t)1) != 1) {
        return 0xFF;
    }
    return Wire.read();
}

static void imuWrite8(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(IMU_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

static bool imuBegin()
{
    uint8_t who;

    imuWrite8(IMU_PWR_MGMT_1, 0x80);   /* reset */
    delay(50);
    imuWrite8(IMU_PWR_MGMT_1, 0x01);   /* zegar z żyroskopu, wyjście z uśpienia */
    delay(10);
    /* Filtr dolnoprzepustowy na 10 Hz. Próbkujemy z częstotliwością 25 Hz,
     * więc bez niego wszystko powyżej 12,5 Hz złożyłoby się na pasmo napadowe:
     * drgania od silnika wibracyjnego i od otoczenia trafiłyby prosto w zakres
     * 2,5–5,5 Hz, którego szuka detektor. */
    imuWrite8(IMU_CONFIG, 0x05);
    /* Wewnętrzna częstotliwość próbkowania 1000/(1+19) = 50 Hz, czyli dwa razy
     * więcej, niż odczytuje program. */
    imuWrite8(IMU_SMPLRT_DIV, 19);
    imuWrite8(IMU_ACCEL_CONF, 0x08);   /* +-4 g */
    who = imuRead8(IMU_WHO_AM_I);
    BT.print(F("BOOT,WHO_AM_I,0x"));
    BT.println(who, HEX);
    return (who != 0x00 && who != 0xFF);
}

/* Moduł wektora przyspieszenia razem z grawitacją, tak jak liczy aplikacja. */
static bool imuReadMagnitude(float *out)
{
    int16_t raw[3];
    uint8_t i;

    Wire.beginTransmission(IMU_ADDR);
    Wire.write(IMU_ACCEL_XOUT);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom((uint8_t)IMU_ADDR, (uint8_t)6) != 6) {
        return false;
    }
    for (i = 0; i < 3; i++) {
        uint8_t hi = Wire.read();
        uint8_t lo = Wire.read();
        raw[i] = (int16_t)((uint16_t)hi << 8 | lo);
    }
    float x = raw[0] / IMU_LSB_PER_G * G_MS2;
    float y = raw[1] / IMU_LSB_PER_G * G_MS2;
    float z = raw[2] / IMU_LSB_PER_G * G_MS2;
    *out = sqrt(x * x + y * y + z * z);
    return true;
}

/* --- wyjścia ----------------------------------------------------------- */

static void ledWrite(const uint8_t rgb[3])
{
#if LED_COMMON_ANODE
    analogWrite(PIN_LED_R, 255 - rgb[0]);
    analogWrite(PIN_LED_G, 255 - rgb[1]);
    analogWrite(PIN_LED_B, 255 - rgb[2]);
#else
    analogWrite(PIN_LED_R, rgb[0]);
    analogWrite(PIN_LED_G, rgb[1]);
    analogWrite(PIN_LED_B, rgb[2]);
#endif
}

static void buzzerSet(bool on)
{
#if EPI_BUZZER_ACTIVE
    digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
#else
    if (on) {
        tone(PIN_BUZZER, 2300);
    } else {
        noTone(PIN_BUZZER);
    }
#endif
}

static void alarmOutput(bool on)
{
    if (alarm_output == on) {
        return;
    }
    alarm_output = on;
    buzzerSet(on);
    digitalWrite(PIN_MOTOR, on ? HIGH : LOW);
}

/* Krótkie potwierdzenie gestu, samym silnikiem. */
static void haptic(uint16_t ms)
{
    digitalWrite(PIN_MOTOR, HIGH);
    delay(ms);
    digitalWrite(PIN_MOTOR, LOW);
}

/* --- transmisja -------------------------------------------------------- */

static void sendEvent(const __FlashStringHelper *name)
{
    BT.print(F("EVT,"));
    BT.print(name);
    BT.print(',');
    BT.println(millis());
}

static void sendTelemetry()
{
    BT.print(F("EPI,"));
    BT.print(millis());
    BT.print(',');
    BT.print(epi_state_name(ctx.state));
    BT.print(',');
    BT.print(ctx.an.amp, 2);
    BT.print(',');
    BT.print(ctx.an.freq, 2);
    BT.print(',');
    BT.print((int)(ctx.hr + 0.5f));
    BT.print(',');
    BT.print((int)(ctx.spo2 + 0.5f));
    BT.print(',');
    BT.print(ctx.soc);
    BT.print(',');
    BT.println((ctx.an.motion_pass ? 1 : 0) | (ctx.an.hr_pass ? 2 : 0) |
               (ctx.an.spo2_pass ? 4 : 0));
}

static void sendConfig()
{
    BT.print(F("CFG,amp,"));      BT.println(ctx.cfg.amp_min, 2);
    BT.print(F("CFG,fmin,"));     BT.println(ctx.cfg.freq_min, 2);
    BT.print(F("CFG,fmax,"));     BT.println(ctx.cfg.freq_max, 2);
    BT.print(F("CFG,hold,"));     BT.println(ctx.cfg.hold_s, 1);
    BT.print(F("CFG,win,"));      BT.println(ctx.cfg.window_s, 1);
    BT.print(F("CFG,hrrise,"));   BT.println(ctx.cfg.hr_rise_pct, 1);
    BT.print(F("CFG,hrslope,"));  BT.println(ctx.cfg.hr_slope_bpm_s, 2);
    BT.print(F("CFG,spo2drop,")); BT.println(ctx.cfg.spo2_drop_pp, 1);
    BT.print(F("CFG,both,"));     BT.println(ctx.cfg.require_both ? 1 : 0);
    BT.print(F("CFG,bio,"));      BT.println(ctx.cfg.require_bio ? 1 : 0);
    BT.print(F("CFG,cool,"));     BT.println(ctx.cfg.cooldown_s, 1);
    BT.print(F("CFG,bright,"));   BT.println(ctx.brightness);
}

static bool setParam(const char *name, float value)
{
    if (!strcmp(name, "amp"))      { ctx.cfg.amp_min = value; }
    else if (!strcmp(name, "fmin"))     { ctx.cfg.freq_min = value; }
    else if (!strcmp(name, "fmax"))     { ctx.cfg.freq_max = value; }
    else if (!strcmp(name, "hold"))     { ctx.cfg.hold_s = value; }
    else if (!strcmp(name, "win"))      { ctx.cfg.window_s = value; }
    else if (!strcmp(name, "hrrise"))   { ctx.cfg.hr_rise_pct = value; }
    else if (!strcmp(name, "hrslope"))  { ctx.cfg.hr_slope_bpm_s = value; }
    else if (!strcmp(name, "spo2drop")) { ctx.cfg.spo2_drop_pp = value; }
    else if (!strcmp(name, "both"))     { ctx.cfg.require_both = (value != 0); }
    else if (!strcmp(name, "bio"))      { ctx.cfg.require_bio = (value != 0); }
    else if (!strcmp(name, "cool"))     { ctx.cfg.cooldown_s = value; }
    else if (!strcmp(name, "bright"))   { ctx.brightness = (uint8_t)value; }
    else { return false; }
    return true;
}

static void triggerSos(uint32_t now)
{
    ctx.sos_active = true;
    sos_sent = false;
    sos_deadline = now + MS_SOS_WINDOW;
    haptic(150);
    sendEvent(F("SOS"));
}

static void handleCommand(char *cmd)
{
    if (!strcmp(cmd, "PING")) {
        BT.println(F("PONG"));
    } else if (!strcmp(cmd, "GET")) {
        sendConfig();
    } else if (!strcmp(cmd, "STATUS")) {
        sendTelemetry();
    } else if (!strcmp(cmd, "CANCEL")) {
        epi_cancel(&ctx, millis());
        sos_sent = false;
        alarmOutput(false);
        sendEvent(F("CANCEL"));
    } else if (!strcmp(cmd, "SOS")) {
        triggerSos(millis());
    } else if (!strcmp(cmd, "ON")) {
        ctx.powered_on = true;
        ctx.state = EPI_IDLE;
        BT.println(F("OK,ON"));
    } else if (!strcmp(cmd, "OFF")) {
        ctx.powered_on = false;
        ctx.state = EPI_OFF;
        alarmOutput(false);
        BT.println(F("OK,OFF"));
    } else if (!strncmp(cmd, "SET,", 4)) {
        char *name = cmd + 4;
        char *sep = strchr(name, ',');
        if (sep) {
            *sep = '\0';
            if (setParam(name, atof(sep + 1))) {
                BT.print(F("OK,"));
                BT.println(name);
                return;
            }
        }
        BT.println(F("ERR,SET"));
    } else if (cmd[0] != '\0') {
        BT.println(F("ERR,CMD"));
    }
}

static void pollCommands()
{
    while (BT.available()) {
        char ch = (char)BT.read();
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            line[line_len] = '\0';
            handleCommand(line);
            line_len = 0;
        } else if (line_len < sizeof(line) - 1) {
            line[line_len++] = ch;
        }
    }
}

/* --- pomiary pomocnicze ------------------------------------------------ */

static uint16_t readBatteryMv()
{
    uint16_t raw = analogRead(PIN_VBAT);
    uint32_t at_pin = ((uint32_t)raw * ADC_VREF_MV) / 1023u;
    return (uint16_t)(at_pin * VBAT_DIV);
}

static void readVitals(uint32_t now)
{
#if EPI_USE_PPG
    ppg.getHeartbeatSPO2();
    float hr = (float)ppg._sHeartbeatSPO2.Heartbeat;
    float spo2 = (float)ppg._sHeartbeatSPO2.SPO2;
    if (hr > 20.0f && hr < 250.0f) {
        epi_push_vitals(&ctx, now, hr, (spo2 > 50.0f) ? spo2 : 0.0f);
    }
#else
    (void)now;
#endif
}

/* --- obsługa zdarzeń --------------------------------------------------- */

static void onButton(epi_btn_event evt, uint32_t now)
{
    switch (evt) {
    case EPI_BTN_CLICK:
        if (ctx.sos_active || ctx.state == EPI_ALARM || ctx.state == EPI_CONFIRMING) {
            epi_cancel(&ctx, now);
            sos_sent = false;
            alarmOutput(false);
            haptic(80);
            sendEvent(F("CANCEL"));
        } else {
            triggerSos(now);
        }
        break;
    case EPI_BTN_LONG:
        ctx.powered_on = !ctx.powered_on;
        ctx.state = ctx.powered_on ? EPI_IDLE : EPI_OFF;
        if (!ctx.powered_on) {
            alarmOutput(false);
        }
        haptic(ctx.powered_on ? 120 : 300);
        sendEvent(ctx.powered_on ? F("ON") : F("OFF"));
        break;
    case EPI_BTN_RESET:
        sendEvent(F("RESET"));
        delay(20);
        asm volatile("jmp 0");   /* miękki restart, bez watchdoga */
        break;
    default:
        break;
    }
}

static void driveAlarm(uint32_t now)
{
    bool active = (ctx.state == EPI_ALARM) || ctx.sos_active;

    if (!active) {
        alarmOutput(false);
        return;
    }
    if ((int32_t)(now - alarm_toggle_at) >= 0) {
        alarmOutput(!alarm_output);
        alarm_toggle_at = now + (alarm_output ? 400u : 600u);
    }
}

/* --- start i pętla ----------------------------------------------------- */

void setup()
{
    pinMode(PIN_BTN, INPUT_PULLUP);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_MOTOR, OUTPUT);
    pinMode(PIN_CHG, INPUT);
    digitalWrite(PIN_MOTOR, LOW);
    buzzerSet(false);

    BT.begin(9600);   /* domyślna prędkość HC-06 */

    Wire.begin();
    /* Biblioteka Wire włącza podciągnięcia wewnętrzne ATmegi, czyli do 5 V,
     * a moduły czujników pracują na 3,3 V. Zapis stanu niskiego na wejściu
     * wyłącza te podciągnięcia; linie podciągają wtedy rezystory na modułach.
     * Nie zastępuje to konwertera poziomów przy montażu na stałe. */
    digitalWrite(SDA, LOW);
    digitalWrite(SCL, LOW);

    epi_cfg cfg;
    epi_cfg_defaults(&cfg);
#if !EPI_USE_PPG
    cfg.require_bio = false;   /* bez pulsoksymetru zostaje tor ruchowy */
#endif
    epi_init(&ctx, &cfg, millis());

    if (!imuBegin()) {
        ctx.fault = true;
        BT.println(F("ERR,IMU"));
    }
#if EPI_USE_PPG
    if (!ppg.begin()) {
        ctx.fault = true;
        BT.println(F("ERR,PPG"));
    } else {
        ppg.sensorStartCollect();
    }
#else
    BT.println(F("WARN,NO_PPG,tryb pokazowy: alarm z samego ruchu"));
#endif

    ctx.soc = epi_batt_tick(&ctx, readBatteryMv());
    last_state = ctx.state;
    haptic(120);
    BT.println(F("BOOT,EPI,1"));
}

void loop()
{
    uint32_t now = millis();

    pollCommands();

    if (now - t_sample >= MS_SAMPLE) {
        t_sample = now;
        float mag;
        if (imuReadMagnitude(&mag)) {
            epi_push_accel(&ctx, mag);
        }
    }

    if (now - t_button >= MS_BUTTON) {
        t_button = now;
        /* Przycisk zwiera do masy, więc stan niski oznacza wciśnięcie. */
        onButton(epi_btn_poll(&ctx, digitalRead(PIN_BTN) == LOW, now), now);
    }

    if (now - t_led >= MS_LED) {
        uint8_t rgb[3];
        t_led = now;
        epi_led_rgb(&ctx, now, rgb);
        ledWrite(rgb);
    }

    if (now - t_vitals >= MS_VITALS) {
        t_vitals = now;
        readVitals(now);
    }

    if (now - t_battery >= MS_BATTERY) {
        t_battery = now;
        ctx.charger_present = (digitalRead(PIN_CHG) == HIGH);
        ctx.soc = epi_batt_tick(&ctx, readBatteryMv());
        if (!ctx.charger_present && ctx.mv_filtered < EPI_BATT_CUTOFF_MV) {
            ctx.powered_on = false;
            ctx.state = EPI_OFF;
            alarmOutput(false);
            sendEvent(F("LOWBATT"));
        }
    }

    /* Automat detekcji chodzi w tym samym rytmie co w aplikacji. */
    static uint32_t t_tick;
    if (now - t_tick >= EPI_TICK_MS) {
        t_tick = now;
        epi_state st = epi_tick(&ctx, now);
        if (st != last_state) {
            last_state = st;
            sendTelemetry();
            if (st == EPI_ALARM) {
                sendEvent(F("ALARM"));
                alarm_toggle_at = now;
            }
        }
    }

    /* Po upływie okna anulowania SOS zostaje wysłane, ale alarm lokalny gra
     * dalej — wycisza go dopiero kliknięcie albo komenda CANCEL. */
    if (ctx.sos_active && !sos_sent && (int32_t)(now - sos_deadline) >= 0) {
        sos_sent = true;
        sendEvent(F("SOS_SENT"));
    }

    driveAlarm(now);

    if (now - t_telemetry >= MS_TELEMETRY) {
        t_telemetry = now;
        sendTelemetry();
    }
}
