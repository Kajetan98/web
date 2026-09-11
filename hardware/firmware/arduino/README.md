# EPI na Arduino Nano z modułem HC-06

Wersja układu na ATmega328P. Ten sam automat detekcji, ta sama dioda i ten sam
przycisk co w wersji na nRF54L15, tyle że bez BLE i bez zarządzania energią.

## Czego ta wersja nie zrobi

HC-06 to Bluetooth Classic z profilem portu szeregowego (SPP). Aplikacja EPI
w `aplikacja/` łączy się przez Web Bluetooth, a Web Bluetooth obsługuje
wyłącznie BLE z profilem GATT. To są dwa różne stosy protokołów w jednym paśmie
2,4 GHz i nie ma między nimi mostu po stronie przeglądarki. Układ z HC-06 nie
połączy się z aplikacją i nie da się tego naprawić po stronie firmware'u.

Rozmawia natomiast z dowolnym terminalem szeregowym Bluetooth na Androidzie
(iOS nie udostępnia SPP aplikacjom innym niż certyfikowane w programie MFi).
Protokół tekstowy jest opisany niżej.

Jeżeli celem jest połączenie z aplikacją, są trzy drogi:

1. Płytka XIAO nRF54L15 z katalogu `../zephyr` — wystawia usługi Heart Rate,
   Pulse Oximeter i Battery, czyli dokładnie to, czego szuka aplikacja. Nic
   w aplikacji nie trzeba zmieniać.
2. Arduino Nano po USB, przez Web Serial. Chrome udostępnia port szeregowy
   stronie internetowej, więc te same linie tekstu, które idą przez HC-06, mogą
   trafić do aplikacji kablem. Wymaga dopisania źródła Web Serial w `app.js`.
3. Moduł BLE zamiast HC-06. Uwaga: popularne moduły HM-10 i ich klony wystawiają
   własną usługę FFE0/FFE1, a nie profile standardowe, więc i tak trzeba zmienić
   filtr w `app.js`.

## Połączenia

Arduino Nano pracuje na 5 V, a wszystkie trzy moduły mają logikę 3,3 V. Między
nimi musi być dwukierunkowy konwerter poziomów na I2C, a moduły powinny mieć
własny stabilizator 3,3 V. Wyjście 3V3 na płytce Nano pochodzi z układu USB
i wydaje kilkadziesiąt miliamperów; sam pulsoksymetr bierze do 15 mA, więc
działa to na granicy i lepiej dać osobny stabilizator.

| Pin Nano | Element | Uwaga |
| --- | --- | --- |
| D0 (RX) | HC-06 TXD | bezpośrednio; na czas wgrywania programu odłączyć |
| D1 (TX) | HC-06 RXD | przez dzielnik 1 kΩ/2 kΩ, wejście modułu jest na 3,3 V |
| D2 | przycisk do masy | podciągnięcie wewnętrzne, bez rezystora zewnętrznego |
| D4 | buzzer | przez tranzystor, moduł bierze do 30 mA |
| D6 | dioda, kanał niebieski | PWM z Timer0 |
| D7 | silnik wibracyjny | MOSFET, dioda gaszeniowa, kondensator 100 nF |
| D9 | dioda, kanał czerwony | PWM z Timer1 |
| D10 | dioda, kanał zielony | PWM z Timer1 |
| A0 | napięcie ogniwa | dzielnik 1:2 z dwóch rezystorów 100 kΩ |
| A1 | wykrycie ładowarki | dzielnik z wyjścia odbiornika Qi, odczyt cyfrowy |
| A4 / A5 | SDA / SCL | przez konwerter poziomów do IMU i pulsoksymetru |
| 5V | HC-06 VCC | moduł ZS-040 ma własny stabilizator |

Dioda ze wspólną anodą: anoda do 5 V, katody przez rezystory do D9, D10 i D6.
Rezystory dobrane osobno dla czerwonej (napięcie przewodzenia około 2 V) i dla
zielonej z niebieską (około 3 V), na prąd 2–3 mA na kanał. Przy diodzie ze
wspólną katodą wystarczy zmienić `LED_COMMON_ANODE` na 0.

W tej wersji akcelerometr ICG-20660L (SEN0443) jest potrzebny, bo Nano nie ma
własnego. Program czyta go przez rejestry zgodne z rodziną InvenSense, więc
zadziała też z MPU-6050. Adres 0x69 przy SDO podciągniętym do zasilania.

Zasilanie: ogniwo Li-Pol daje 3,7 V, a Nano potrzebuje 5 V, więc między nimi
musi być przetwornica podwyższająca. Napięcie 5 V podaje się na pin 5V, nie na
Vin, bo stabilizator na płytce potrzebuje co najmniej 7 V.

## Ustawienie modułu HC-06

Moduł odpowiada na komendy AT tylko wtedy, kiedy nie jest z niczym sparowany.
Domyślnie 9600 bodów, 8N1. Klasyczne HC-06 przyjmują komendy bez znaku końca
linii, z odstępem około sekundy; część egzemplarzy ZS-040 z nowszym firmware
wymaga CR+LF. Sprawdź reakcję na samo `AT` (powinno odpowiedzieć `OK`) i dopiero
potem ustawiaj resztę.

```
AT            -> OK
AT+NAMEEPI    -> OKsetname
AT+PIN1234    -> OKsetPIN
AT+BAUD4      -> OK9600
```

Program pracuje na 9600 bodach. Wyższe prędkości nie są potrzebne: jedna linia
telemetrii to około 50 bajtów raz na sekundę.

## Protokół

Linie kończone znakiem nowej linii, pola rozdzielone przecinkami.

Z urządzenia:

| Linia | Znaczenie |
| --- | --- |
| `EPI,<ms>,<stan>,<amp>,<freq>,<hr>,<spo2>,<soc>,<flagi>` | telemetria, raz na sekundę i przy każdej zmianie stanu |
| `EVT,<nazwa>,<ms>` | zdarzenie: ALARM, CANCEL, SOS, SOS_SENT, ON, OFF, LOWBATT, RESET |
| `CFG,<nazwa>,<wartość>` | odpowiedź na `GET` |
| `BOOT,...`, `WARN,...`, `ERR,...` | komunikaty startowe i błędy |

Flagi to suma bitów: 1 kryterium ruchowe, 2 kryterium tętna, 4 kryterium SpO₂.
Amplituda w m/s², częstotliwość w Hz, oba z dwoma miejscami po przecinku.

Do urządzenia:

| Komenda | Działanie |
| --- | --- |
| `PING` | odpowiada `PONG` |
| `STATUS` | wysyła linię telemetrii |
| `GET` | wypisuje całą konfigurację |
| `SET,<nazwa>,<wartość>` | zmienia próg: amp, fmin, fmax, hold, win, hrrise, hrslope, spo2drop, both, bio, cool, bright |
| `SOS` | wywołuje SOS z oknem anulowania |
| `CANCEL` | kasuje alarm albo przerywa okno potwierdzenia |
| `ON`, `OFF` | włącza i wyłącza monitorowanie |

Ustawienia nie są zapisywane w pamięci nieulotnej; po restarcie wracają
wartości domyślne.

## Opcje kompilacji

Na początku `EpiNano.ino`:

| Stała | Domyślnie | Znaczenie |
| --- | --- | --- |
| `EPI_BT_HARDWARE_SERIAL` | 1 | HC-06 na sprzętowym UART. Zero przenosi transmisję na SoftwareSerial (D8 i D12) i zwalnia USB, kosztem dokładności czasu: SoftwareSerial wyłącza przerwania na czas nadawania każdego bajtu, więc `millis()` gubi kilka milisekund na każdą wysłaną linię |
| `EPI_BUZZER_ACTIVE` | 1 | buzzer z generatorem, wystarczy podać napięcie. Zero włącza sterowanie funkcją `tone()`, która zajmuje Timer2 i wyłącza PWM na D3 i D11 |
| `EPI_USE_PPG` | 0 | bez pulsoksymetru urządzenie startuje w trybie pokazowym, w którym do alarmu wystarcza kryterium ruchowe. Jedynka włącza obsługę modułu MAX30102 przez bibliotekę DFRobot |

Tryb pokazowy istnieje po to, żeby układ dało się uruchomić i zobaczyć działanie
diody, przycisku i transmisji bez kompletu czujników. Fałszywych alarmów jest
w nim dużo, bo odpada cała część automatu, która je odsiewa.

## Ograniczenia tej wersji

Czas pracy. Sam Nano bierze około 19 mA, HC-06 w połączeniu kilkanaście do
kilkudziesięciu, pulsoksymetr do 15 mA. Razem rzędu 50–70 mA bez trybu
uśpienia, więc z ogniwa 980 mAh wychodzi kilkanaście godzin. Wersja na
nRF54L15 pracuje w tym samym zastosowaniu z prądem około 1,2 mA. Różnica bierze
się z Bluetooth Classic, z braku uśpienia i ze stabilizatora oraz układu USB na
płytce Nano.

Pamięć. ATmega328P ma 32 kB pamięci programu (30720 B po odjęciu bootloadera)
i 2 kB pamięci RAM. Zmierzone zużycie po konsolidacji:

| Wariant | Program | Dane statyczne |
| --- | --- | --- |
| UART sprzętowy, buzzer z generatorem | 18808 B (57 %) | 1235 B (60 %) |
| SoftwareSerial, buzzer sterowany tone() | 21002 B (64 %) | 1298 B (63 %) |

Sam kontekst urządzenia to 469 bajtów, resztę zajmują bufory Serial, Wire
i zmienne rdzenia Arduino. Na stos zostaje około 800 bajtów, więc dołożenie
zapisu na kartę albo dłuższej historii wymaga przeliczenia budżetu, a nie
samego dopisania kodu.

Precyzja czasu. `millis()` wystarcza do okna 4 s i odliczania 15 s, ale nie ma
tu zegara czasu rzeczywistego, więc zdarzenia mają znacznik liczony od włączenia
zasilania. Znacznik bezwzględny musi dostawić odbiorca po drugiej stronie
łącza.

## Zgodność z wersją na nRF54L15

Progi, priorytety diody, barwy, tablica napięć ogniwa i gesty przycisku są
wspólne dla obu wersji. Pilnuje tego `tests/test_nano.c`, który podaje obu
implementacjom ten sam sygnał i porównuje wyniki:

```
cd ../tests && make test
```

Test wyłapuje rozjechanie się wersji przy zmianie w jednej z nich. Przy pisaniu
tego kodu wykrył różnicę o jedną próbkę w oknie analizy, która przesuwała
mierzoną częstotliwość o 2,6 procent.

## Co zostało sprawdzone

Szkic kompiluje się i konsoliduje dla ATmega328P przeciwko rdzeniowi
ArduinoCore-avr, w obu wariantach opcji z tabeli wyżej; stąd pochodzą liczby
zużycia pamięci. Logika przechodzi testy porównawcze z wersją na nRF54L15.

Nie sprawdzone na sprzęcie, bo wymaga płytki: odczyt akcelerometru (rejestry
rodziny InvenSense, program wypisuje przy starcie zawartość WHO_AM_I do
weryfikacji), obsługa pulsoksymetru przez bibliotekę DFRobot, komendy AT modułu
HC-06 oraz dobór rezystorów diody.
