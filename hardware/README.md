# EPI — warstwa sprzętowa pre-prototypu

Dokument projektowy urządzenia noszonego EPI: dobór części, rozkład wyprowadzeń,
zachowanie diody RGB i przycisku, profil BLE zgodny z aplikacją w `aplikacja/`
oraz decyzja o języku firmware'u. Kod, który da się uruchomić i przetestować bez
płytki, leży w `firmware/`.

Etap: pre-prototyp inżynierski (TRL 2→3). Urządzenie nie jest wyrobem medycznym
i nie zastępuje opieki lekarskiej.

## Rozstrzygnięcia z briefu

| Pytanie | Rozstrzygnięcie | Powód |
| --- | --- | --- |
| ESP32 czy XIAO nRF54L15 Sense? | nRF54L15 | brief wymienia obie płytki; aplikacja łączy się przez BLE, więc Wi-Fi z ESP32 nie jest do niczego potrzebne, a pobór prądu w uśpieniu jest o dwa rzędy wielkości wyższy |
| IMU ICG-20660L (SEN0443) | pomiń w wersji na nRF54L15 | wersja Sense ma na płytce LSM6DS3TR-C, ten sam typ czujnika; w wariancie na Arduino Nano ten moduł jest potrzebny |
| XIAO Logger HAT | kup, ale do stanowiska pomiarowego | RTC i karta microSD są potrzebne do zbierania zbioru uczącego, czujniki środowiskowe nie wnoszą nic do detekcji napadu |
| OpenLog (ATmega328 + microSD) | pomiń | dubluje funkcję Logger HAT-a, a nie ma RTC ani dzielnika napięcia |
| Buzzer 5 V 12 mm THT | zamień na przetwornik 3 V sterowany PWM | 5 V wymaga osobnej szyny, ma jeden stały ton, 12 mm to dużo jak na opaskę |
| C czy MicroPython? | C na Zephyrze (nRF Connect SDK) na urządzeniu, Python na komputerze do analizy i uczenia | powody w rozdziale o firmwarze |
| Moduł HC-06 | działa, ale nie z aplikacją | HC-06 to Bluetooth Classic (SPP), a aplikacja używa Web Bluetooth, czyli wyłącznie BLE; szczegóły w rozdziale o wariancie na Arduino Nano |
| Uproszczony model AI/ML | trzy etapy: reguła, klasyfikator liniowy z cech, ewentualnie mała sieć | rozdział o modelu |

## Platforma

Seeed XIAO nRF54L15 Sense (Seeedstudio 101991422): nRF54L15, Cortex-M33 128 MHz,
1,5 MB pamięci nieulotnej, BLE 6.0, Thread, Zigbee, Matter, NFC, 16 wyprowadzeń
GPIO, ładowarka ogniwa na płytce, w wersji Sense akcelerometr z żyroskopem
LSM6DS3TR-C oraz mikrofon cyfrowy.

Argument za tą płytką zamiast ESP32: cały ruch z urządzenia do telefonu idzie
przez BLE (aplikacja używa Web Bluetooth), a urządzenie ma pracować całą dobę
z ogniwa 980 mAh. ESP32 w trybie połączonego BLE pobiera rzędy wielkości więcej
niż nRF, więc wybór ESP32 oznaczałby ładowanie codziennie zamiast raz na
kilka dni. Jeżeli w przyszłości pojawi się wymaganie Wi-Fi (wysyłka zdarzeń bez
telefonu), właściwym krokiem jest osobny moduł, nie zamiana rdzenia.

Mikrofon z wersji Sense nie jest w tej wersji używany. Warto go zostawić jako
zapas: dźwięk w trakcie napadu toniczno-klonicznego jest osobnym torem
potwierdzenia, ale wymaga własnego zbioru danych i podnosi ryzyko prywatności.

## Lista części

| Część | Symbol | Werdykt | Uwaga |
| --- | --- | --- | --- |
| XIAO nRF54L15 Sense | 101991422 | podstawa pre-prototypu | IMU i mikrofon na płytce |
| Fermion ICG-20660L | SEN0443 | pomiń przy nRF54L15, potrzebny przy Arduino Nano | dubluje LSM6DS3TR-C tylko na płytce Sense |
| Fermion MAX30102 V2.0 | SEN0344 | kup, z zastrzeżeniami | 3,3 V, I2C 0x57, poniżej 15 mA, 18 × 22 mm |
| Akumulator Li-Pol 980 mAh 1S | Akyga | do stanowiska, nie do opaski | 50 × 34 × 6 mm, trzy przewody |
| XIAO Logger HAT | 114993446 | tylko do zbierania danych | SHT40, BH1750, PCF8563, microSD do 32 GB, dzielnik napięcia baterii |
| Buzzer z generatorem 5 V 12 mm | 786 | zamień | 5 V, 85 dB, do 30 mA, ton 2,3 kHz ±500 Hz |
| Silnik wibracyjny MT35 3 V | 4727 | kup, przez tranzystor | około 90 mA, 9 × 5,2 × 5,2 mm |
| Tact switch 6 × 6 mm | 381 | kup | wymaga membrany w szczelnej obudowie |
| OpenLog ATmega328 | — | pomiń | funkcja pokryta przez Logger HAT |
| Odbiornik Qi 5 V | do wyboru | kup razem z układem ładowania | patrz rozdział o zasilaniu |
| Moduł Bluetooth HC-06 ZS-040 | — | tylko do wariantu na Arduino Nano | Bluetooth Classic SPP, nie łączy się z aplikacją |
| Dioda RGB wspólna anoda | do wyboru | kup, 3 × PWM | nie WS2812B |

Kilka pozycji wymaga komentarza.

Pulsoksymetr. Moduł DFRobot ma własny mikrokontroler z algorytmem tętna i SpO₂
i oddaje gotowe wartości po I2C albo UART. To wygodne, ale trzeba sprawdzić dwie
rzeczy przed zamówieniem większej liczby sztuk: czy biblioteka udostępnia surowe
próbki PPG (bez nich aplikacja nie ma czego rysować w oknie −30 s/+60 s wokół
zdarzenia) oraz po jakim czasie od włączenia moduł podaje stabilny odczyt. Jeżeli
ten czas przekracza okno potwierdzenia (domyślnie 15 s), kryterium biometryczne
nie zdąży się wypełnić i trzeba wziąć moduł oddający surowe próbki.

Osobna sprawa to miejsce pomiaru. MAX30102 jest przeznaczony do palca i płatka
ucha. Pomiar SpO₂ na nadgarstku jest niepewny, a w trakcie drgań, czyli dokładnie
wtedy, kiedy algorytm go potrzebuje, artefakty ruchowe są największe. Przy
domyślnych progach kryterium biometryczne jest alternatywą (tętno albo SpO₂), co
częściowo to obchodzi, ale przy walidacji trzeba założyć, że SpO₂ z nadgarstka
jest sygnałem pomocniczym, nie rozstrzygającym.

Akumulator. 980 mAh wystarcza z zapasem, ale kostka 50 × 34 × 6 mm nie zmieści
się w opasce. Do pre-prototypu na stole jest w porządku; do wersji noszonej
trzeba ogniwa rzędu 300–500 mAh o innej geometrii. Trzeci przewód to zwykle
termistor: warto go wykorzystać, jeżeli układ ładowania ma wejście pomiaru
temperatury, bo ładowanie indukcyjne grzeje ogniwo od strony cewki.

Logger HAT kontra OpenLog. Logger HAT wchodzi pod płytkę XIAO, ma zegar czasu
rzeczywistego PCF8563 z podtrzymaniem z ogniwa CR1220, kartę microSD i dzielnik
napięcia baterii. Do zbierania zbioru uczącego to dokładnie ten zestaw: surowe
próbki z IMU i PPG na kartę, ze znacznikiem czasu, który da się zestawić
z nagraniem wideo albo z notatkami. Do urządzenia noszonego nie wchodzi:
temperatura, wilgotność i natężenie światła nie mówią nic o napadzie, a zapis na
kartę kosztuje energię i miejsce. OpenLog robi mniej (tylko zrzut z UART na
kartę) i przy Logger HAT-cie jest zbędny.

Dioda. Odpada WS2812B: układ sterownika pobiera rzędu 1 mA nawet przy zgaszonej
diodzie, czyli tyle, ile cały budżet spoczynkowy urządzenia. Zwykła dioda RGB ze
wspólną anodą, trzy rezystory i trzy kanały PWM kosztują trzy wyprowadzenia
i nie pobierają nic, kiedy nie świecą.

## Rozkład wyprowadzeń

| Pin | Funkcja | Uwaga |
| --- | --- | --- |
| D0 | przycisk | podciągnięcie do zasilania, wybudzanie z System OFF |
| D1 | dioda, kanał czerwony | PWM |
| D2 | dioda, kanał zielony | PWM |
| D3 | dioda, kanał niebieski | PWM |
| D4 | SDA | pulsoksymetr 0x57 |
| D5 | SCL | |
| D6 | buzzer | przez tranzystor |
| D7 | silnik wibracyjny | tranzystor MOSFET, dioda gaszeniowa, kondensator 100 nF |
| D8 | przerwanie z pulsoksymetru | jeżeli moduł je wyprowadza |
| D9 | stan ładowania | wyjście STAT układu ładowania |
| D10 | wykrycie ładowarki | dzielnik z wyjścia odbiornika Qi |
| ADC | napięcie ogniwa | dzielnik na płytce XIAO, kanał do sprawdzenia w dokumentacji |

Numery wyprowadzeń w plikach devicetree trzeba wpisać z pinoutu płytki; overlay
w `firmware/zephyr/boards/` ma w tych miejscach komentarz zamiast wartości.

Rozkład jest ciasny. Jeżeli okaże się, że potrzebny jest osobny sygnał do
sterowania zasilaniem pulsoksymetru, pierwszym kandydatem do zwolnienia jest D8
(odpytywanie zamiast przerwania).

## Zasilanie i ładowanie

Ładowanie indukcyjne wynika z założenia szczelnej obudowy: brak złącza to brak
najsłabszego punktu mechanicznego i najczęstszej drogi wejścia wilgoci. Odbiornik
Qi daje 5 V, więc ścieżka wygląda tak: cewka i odbiornik Qi, układ ładowania
Li-Pol, ogniwo, płytka.

Ładowarka na płytce XIAO działa, ale prąd ładowania w rodzinie XIAO to typowo
50 albo 100 mA. Przy 980 mAh oznacza to ładowanie przez kilkanaście godzin.
Trzeba sprawdzić wartość dla nRF54L15 w dokumentacji płytki; jeżeli potwierdzi
się 100 mA, sensowniejszy jest osobny układ ładowania zasilany z odbiornika Qi,
z prądem 200–300 mA i z wejściem termistora, a złącze baterii na XIAO zostaje
tylko jako wejście zasilania.

Szacunek poboru prądu. Wartości są do zmierzenia na gotowym egzemplarzu, tutaj
służą do wskazania, co dominuje.

| Blok | Spoczynek | Alarm |
| --- | --- | --- |
| nRF54L15 z połączeniem BLE | 0,02–0,05 mA | około 1 mA |
| LSM6DS3TR-C, akcelerometr 52 Hz | 0,05–0,2 mA | 0,6 mA z żyroskopem |
| Pulsoksymetr, poniżej 15 mA przy 7 % wypełnienia | około 1 mA | 15 mA |
| Dioda RGB | 0,03 mA | 2,5 mA |
| Buzzer | 0 | 30 mA |
| Silnik wibracyjny | 0 | 90 mA |
| Razem | około 1,1–1,3 mA | około 140 mA |

Przy 1,2 mA i 85 % wykorzystanej pojemności wychodzi około 700 godzin, czyli
niecały miesiąc. To górna granica: nie uwzględnia strat na płytce, ponownych
połączeń BLE ani fałszywych wejść w stan SUSPECT, które budzą pulsoksymetr.
Realistycznie należy zakładać kilka do kilkunastu dni i zmierzyć to na
urządzeniu.

Jedna liczba warta zapamiętania: pulsoksymetr włączony na stałe to 15 mA, czyli
około 65 godzin pracy. Sterowanie jego wypełnieniem jest najważniejszą decyzją
energetyczną w całym projekcie. Dlatego automat detekcji ma stan SUSPECT: PPG
pracuje rzadko, a przechodzi na pełne próbkowanie dopiero, gdy tor ruchowy coś
znajdzie.

Firmware wyłącza urządzenie przy 3400 mV (`EPI_BATT_CUTOFF_MV`), żeby nie zejść
do napięcia, przy którym ogniwo traci pojemność.

## Dioda RGB

Brief podaje sześć reguł. Trzy z nich (zielony po włączeniu, zielony przy pełnym
ogniwie, płynne przejście z zielonego na czerwony w miarę rozładowania) opisują
ten sam kanał informacji, a jedna barwa, czerwień, oznacza dwie różne rzeczy
(rozładowane ogniwo i alarm). Rozstrzygnięcie: barwa niesie poziom naładowania,
a rytm błysków rozróżnia stany. Świecenie ciągłe zarezerwowane jest dla
ładowarki, kiedy energia nie jest problemem.

| Priorytet | Warunek | Barwa | Rytm |
| --- | --- | --- | --- |
| 1 | alarm potwierdzony albo SOS z przycisku | czerwony | 4 Hz, wypełnienie 50 % |
| 2 | okno potwierdzenia | bursztynowy | 2 Hz, błysk 120 ms |
| 3 | na ładowarce, ogniwo pełne | zielony | ciągły |
| 4 | na ładowarce, ładowanie w toku | niebieski | oddech, okres 3 s |
| 5 | awaria czujnika albo zapisu | magenta | 3 błyski co 5 s |
| 6 | pierwsze 2,5 s po włączeniu | barwa z poziomu naładowania | ciągły |
| 7 | poziom naładowania ≤ 10 % | czerwony | 2 błyski co 5 s |
| 8 | praca normalna | od zielonego do czerwonego wg poziomu | 1 błysk 30 ms co 5 s |
| 9 | wyłączone | — | zgaszona |

Alarm świeci pełną jasnością niezależnie od ustawienia jasności; reszta stanów
podlega ustawieniu (domyślnie 60/255, żeby dioda nie raziła w nocy).

Reguła z briefu „zielony, kiedy włączona" realizuje się przez wiersze 6 i 8: po
włączeniu dioda przez 2,5 s pokazuje poziom naładowania, a przy pełnym ogniwie ta
barwa jest zielona. Reguła „czerwony, kiedy rozładowana" to wiersz 7, odróżniony
od alarmu rytmem: alarm miga cztery razy na sekundę, niska bateria daje dwa
krótkie błyski raz na pięć sekund.

Przejście barwy. Poziom naładowania jest przeliczany na odcień: 100 % to 120
stopni (zielony), 10 % i mniej to 0 stopni (czerwony), pomiędzy interpolacja
liniowa przez żółty i pomarańczowy. Wartości wypełnienia PWM z
`epi_led_soc_color()`:

| Poziom | R | G | B |
| --- | --- | --- | --- |
| 100 % | 0 | 115 | 0 |
| 80 % | 115 | 115 | 0 |
| 60 % | 230 | 115 | 0 |
| 40 % | 255 | 77 | 0 |
| 20 % | 255 | 25 | 0 |
| 10 % i mniej | 255 | 0 | 0 |

Kanał zielony jest przyduszony współczynnikiem kalibracyjnym
(`EPI_LED_GAIN_G`), bo zielona struktura w typowej diodzie RGB ma wyższą
skuteczność świetlną niż czerwona i przy równym wypełnieniu mieszanka wychodzi
zielonkawa. Procedura kalibracji: ustawić `EPI_LED_GAIN_*` na 255, wyświetlić
kolejno czysty czerwony, zielony i niebieski przy tym samym wypełnieniu,
zmierzyć jasność (wystarczy aparat telefonu w trybie ręcznym, ta sama ekspozycja)
i dobrać współczynniki tak, żeby trzy barwy miały zbliżoną jasność. Rezystory
szeregowe dobiera się osobno dla czerwonej (napięcie przewodzenia około 2 V)
i dla zielonej z niebieską (około 3 V), na prąd 2–3 mA na kanał.

Dlaczego błyski, a nie świecenie ciągłe. Dioda świecąca bez przerwy przy 3 mA to
72 mAh na dobę, czyli ponad 7 % pojemności ogniwa dziennie. Błysk 30 ms co 5 s
to wypełnienie 0,6 %, czyli poniżej 1 mAh na dobę. Różnica decyduje o tym, czy
urządzenie ładuje się raz w tygodniu, czy co drugi dzień.

## Przycisk

| Gest | Czas | Działanie |
| --- | --- | --- |
| kliknięcie | poniżej 0,7 s | w spoczynku: SOS z oknem anulowania 10 s; w oknie potwierdzenia albo w alarmie: anulowanie |
| przytrzymanie | od 2 s | włączenie albo wyłączenie urządzenia |
| przytrzymanie | od 8 s | twardy restart |

Filtrowanie drgań styku: 30 ms. Po każdym rozpoznanym geście silnik wibracyjny
daje krótkie potwierdzenie, inne dla włączenia (120 ms), SOS (150 ms),
anulowania (80 ms) i wyłączenia (300 ms), żeby dało się obsłużyć urządzenie bez
patrzenia na diodę.

Wyłączone urządzenie siedzi w trybie System OFF i budzi się poziomem na
wyprowadzeniu przycisku, więc ten sam przycisk włącza i wyłącza.

Kliknięcie jako SOS jest wygodne i ryzykowne naraz: przycisk łatwo nacisnąć
przypadkowo. Zabezpieczeniem jest okno anulowania i wgłębienie w obudowie.
Jeżeli w testach pojawi się dużo przypadkowych SOS, alternatywą jest podwójne
kliknięcie, kosztem czasu reakcji w trakcie aury.

## Alarm lokalny

Buzzer z briefu jest przetwornikiem z generatorem: podanie 5 V daje stały ton
2,3 kHz i 85 dB, do 30 mA. Wady w tym zastosowaniu: wymaga szyny 5 V (na 3,3 V
zagra ciszej albo wcale), nie da się zmienić tonu ani zagrać wzorca, a 12 mm
średnicy i 9,5 mm wysokości to dużo w obudowie na nadgarstek. Zamiennik:
przetwornik magnetoelektryczny albo piezo bez generatora, sterowany PWM
z częstotliwością rezonansową, przez tranzystor. Wtedy alarm może brzmieć inaczej
niż potwierdzenie SOS, a to jest istotne dla osoby w otoczeniu.

Szczelna obudowa tłumi dźwięk. Potrzebny jest otwór akustyczny zamknięty membraną
oddychającą, inaczej głośność spada o kilkanaście decybeli.

Silnik MT35 pobiera około 90 mA, czyli więcej, niż wyprowadzenie mikrokontrolera
może dać. Sterowanie przez tranzystor MOSFET z małym napięciem progowym, z diodą
gaszeniową równolegle do silnika i kondensatorem 100 nF na jego zaciskach.

Zależność, o której łatwo zapomnieć: silnik i buzzer wprowadzają drgania do tej
samej obudowy, w której siedzi akcelerometr, a ruch obudowy psuje sygnał PPG.
W trakcie pracy wyjść alarmu detektor nie powinien wyciągać wniosków z okna
analizy; w firmwarze jest na to miejsce w `alarm_work_fn`.

## Łączność

Aplikacja w `aplikacja/app.js` filtruje urządzenia po usługach Heart Rate
i Pulse Oximeter, a opcjonalnie czyta Battery Service. Urządzenie musi więc
wystawiać profile standardowe, nie tylko własne.

| Usługa | UUID | Charakterystyka | Zastosowanie |
| --- | --- | --- | --- |
| Heart Rate | 0x180D | 0x2A37, powiadomienia | tętno, po tym filtruje aplikacja |
| Pulse Oximeter | 0x1822 | 0x2A5F, powiadomienia, SFLOAT | SpO₂ i tętno |
| Battery | 0x180F | 0x2A19 | poziom naładowania |
| EPI | 6e5a0001-b5a3-f393-e0a9-e50e24dcca9e | 6e5a0002-…, powiadomienia | stan automatu, amplituda, częstotliwość, nachylenie tętna |

Format ramki stanu: bajt stanu, bajt flag kryteriów (ruch, tętno, SpO₂), po dwa
bajty na amplitudę w setnych m/s², częstotliwość w setnych Hz i nachylenie tętna
w setnych bpm/s.

Aplikacja na razie nie czyta usługi własnej; obsługa stanu urządzenia po stronie
`app.js` to osobne zadanie. Do czasu jej dodania aplikacja liczy własny automat
z akcelerometru telefonu, a z opaski bierze tylko tętno i SpO₂.

Parowanie z zapisem kluczy (`CONFIG_BT_SMP`, `CONFIG_BT_BONDABLE`) jest
włączone: powiadomienie o napadzie nie powinno być dostępne dla dowolnego
urządzenia w zasięgu.

## Firmware

C na Zephyrze w nRF Connect SDK. Powody, po kolei:

Zephyr ma płytkę XIAO nRF54L15 w drzewie i gotowe usługi BLE (Heart Rate,
Battery), więc do napisania zostaje usługa pulsoksymetru i własna. Tryb System
OFF z wybudzeniem przyciskiem, sterowanie zasilaniem czujników i budziki
z rozdzielczością milisekundy są dostępne wprost, a od nich zależy czas pracy.
Okno analizy 4 s przy 52 Hz liczone co 250 ms to operacja, którą warto mieć
przewidywalną czasowo.

MicroPython na XIAO nRF54L15 istnieje (Seeed publikuje obraz i przykłady), więc
nie jest to wybór między działającym a niedziałającym. Odpada z powodu trzech
rzeczy: serwer GATT z własnymi usługami, zarządzanie energią i deterministyczne
próbkowanie. Do napisania czujnika na biurku byłby szybszy; do urządzenia, które
ma pracować tydzień na ogniwie i wysłać powiadomienie o napadzie, nie.

Python zostaje po stronie komputera i tam jest niezastąpiony: analiza nagranych
przebiegów, dobór progów, uczenie klasyfikatora, generowanie tablicy
współczynników do wklejenia w firmware. Podział wygląda tak, że algorytm
powstaje w Pythonie na danych z karty, a na urządzenie trafia tylko wynik.

Układ katalogu:

| Ścieżka | Zawartość |
| --- | --- |
| `firmware/lib/` | logika bez zależności od sprzętu: dioda, bateria, przycisk, detektor |
| `firmware/tests/` | testy tej logiki oraz porównanie obu wersji, uruchamiane na komputerze (`make test`) |
| `firmware/zephyr/` | integracja z nRF54L15: devicetree, konfiguracja, czujniki, BLE |
| `firmware/arduino/` | szkic na Arduino Nano z modułem HC-06 |

Podział jest celowy. `lib/` kompiluje się zwykłym `cc`, więc zachowanie diody,
progi baterii, gesty przycisku i cały automat detekcji można sprawdzić bez
płytki, a te same pliki wchodzą do kompilacji Zephyra. `firmware/zephyr/` jest
na dziś szkieletem: wymaga uzupełnienia numerów wyprowadzeń i sterownika
pulsoksymetru, i nie był kompilowany przeciwko SDK.

Detektor w `lib/epi_detector.c` to port automatu z `aplikacja/app.js`: te same
stany, te same cechy sygnału i te same progi domyślne (amplituda 2,5 m/s², pasmo
2,5–5,5 Hz, utrzymanie 3 s, okno potwierdzenia 15 s, wzrost tętna 35 % przy
nachyleniu co najmniej 1,2 bpm/s, spadek SpO₂ o 4 punkty, wyciszenie 45 s).
Dzięki temu urządzenie i aplikacja rozstrzygają tak samo, a rozbieżność
w testach oznacza błąd, a nie różnicę implementacji.

## Wariant na Arduino Nano

Poza wersją docelową w katalogu `firmware/arduino/` leży kompletny szkic na
Arduino Nano z modułem Bluetooth HC-06. Powstał jako układ do testów na stole:
te same progi, ta sama dioda i ten sam przycisk, ale bez BLE i bez trybów
oszczędzania energii.

Jedna rzecz jest w nim rozstrzygnięta z góry i nie da się jej obejść w kodzie.
HC-06 rozmawia profilem portu szeregowego (SPP) przez Bluetooth Classic,
a aplikacja EPI łączy się przez Web Bluetooth, który obsługuje wyłącznie BLE
z profilem GATT. To dwa różne stosy protokołów i przeglądarka nie ma między nimi
mostu, więc układ z HC-06 nie połączy się z aplikacją. Rozmawia za to
z dowolnym terminalem szeregowym Bluetooth na Androidzie, protokołem tekstowym
opisanym w `firmware/arduino/README.md`.

Drogi, które dają połączenie z aplikacją: płytka nRF54L15 (bez zmian
w aplikacji), Arduino Nano po USB przez Web Serial (wymaga dopisania źródła
w `app.js`), albo moduł BLE zamiast HC-06 (moduły HM-10 wystawiają własną
usługę FFE0, więc filtr w aplikacji i tak trzeba zmienić).

Porównanie obu wersji:

| | XIAO nRF54L15 Sense | Arduino Nano + HC-06 |
| --- | --- | --- |
| Łączność | BLE, profile standardowe | Bluetooth Classic SPP, tekst |
| Praca z aplikacją EPI | tak | nie |
| Pobór prądu w spoczynku | około 1,2 mA | 50–70 mA |
| Czas pracy z ogniwa 980 mAh | kilka do kilkunastu dni | kilkanaście godzin |
| Akcelerometr | na płytce | osobny moduł, SEN0443 |
| Napięcie logiki | 3,3 V, zgodne z czujnikami | 5 V, potrzebny konwerter poziomów |
| Pamięć RAM | 256 kB | 2 kB, zajęte w 60 % |
| Zastosowanie | urządzenie noszone | stanowisko testowe, pokaz działania |

## Model AI/ML

Napisane w briefie „uproszczony model AI/ML" da się rozłożyć na trzy etapy, przy
czym pre-prototyp potrzebuje tylko pierwszego.

Etap pierwszy to reguła, która jest już w `epi_detector.c`: w oknie 4 s liczona
jest amplituda RMS przyspieszenia po usunięciu składowej stałej oraz
częstotliwość dominująca z liczby przejść przez zero, a decyzja zapada, gdy oba
mieszczą się w zakresie i utrzymują przez 3 s. Nie wymaga danych uczących, da się
prześledzić linijka po linijce i wystarcza do pierwszych testów na stole.

Etap drugi to klasyfikator na cechach. Z tego samego okna 4 s: RMS na każdej osi
i na module, moc w pasmach 1–3, 3–6 i 6–10 Hz, częstotliwość dominująca, szerokość
piku widma, liczba przejść przez zero, szczyt autokorelacji, zmienność
amplitudy między kolejnymi oknami, do tego różnica tętna względem linii bazowej
i jego nachylenie. Razem kilkanaście liczb. Model: regresja logistyczna albo
niewielki las drzew o głębokości 4, uczony w scikit-learn na komputerze,
wyeksportowany jako tablica współczynników w pliku nagłówkowym. Koszt wykonania
na nRF54L15 to mikrosekundy, zajętość pamięci poniżej kilobajta. To jest
poziom, który w tym projekcie ma sens: daje kalibrowany próg zamiast ręcznie
dobranych stałych, a przy tym da się pokazać, dlaczego zapadła taka decyzja.

Etap trzeci, mała sieć splotowa jednowymiarowa na surowym oknie sygnału przez
TensorFlow Lite Micro, ma sens dopiero wtedy, gdy zbiór danych będzie na tyle
duży, że klasyfikator na cechach przestanie się poprawiać. Wcześniej to głównie
sposób na przeuczenie się na kilkunastu nagraniach.

Zbieranie danych. Tu wchodzi Logger HAT: XIAO z HAT-em, IMU i pulsoksymetrem
zapisuje surowe próbki na kartę ze znacznikiem czasu z PCF8563, a przycisk służy
do wstawiania znaczników. Przypadki negatywne da się zebrać samemu i to od nich
trzeba zacząć, bo to one generują fałszywe alarmy: mycie zębów, szczotkowanie
włosów, bieg, jazda po nierównej drodze, klaskanie, wkręcanie wkrętarką,
trzęsienie ręką celowo. Przypadków pozytywnych nie da się zebrać samodzielnie
i to jest twarde ograniczenie projektu: albo współpraca z ośrodkiem klinicznym,
albo publiczny zbiór nagrań z akcelerometru. Znane publiczne zbiory napadów
(na przykład CHB-MIT) są zapisami EEG, więc do toru ruchowego nie pasują.

Do czasu zdobycia danych pozytywnych każda liczba opisująca czułość i swoistość
jest niepoparta i nie powinna pojawiać się w opisie projektu.

## Do sprawdzenia i zmierzenia

1. Prąd pobierany w spoczynku i w alarmie, na gotowym egzemplarzu.
2. Prąd ładowania ładowarki na płytce XIAO nRF54L15 i decyzja o osobnym układzie.
3. Czy moduł SEN0344 oddaje surowe próbki PPG i po jakim czasie podaje stabilny
   odczyt (musi zmieścić się w oknie potwierdzenia).
4. Jakość PPG na nadgarstku w ruchu; jeżeli SpO₂ okaże się bezużyteczne
   w drganiach, kryterium biometryczne trzeba oprzeć na tętnie i perfuzji.
5. Wpływ pracy silnika i buzzera na akcelerometr i PPG; wielkość okna wyciszenia
   detektora w trakcie alarmu.
6. Głośność buzzera w zamkniętej obudowie z otworem akustycznym.
7. Zachowanie przycisku pod membraną, po kilku tysiącach naciśnięć.
8. Krzywa rozładowania konkretnego ogniwa, do zastąpienia tablicy w
   `epi_battery.c`.
9. Temperatura obudowy w trakcie ładowania indukcyjnego, przy ogniwie i przy
   skórze.

## Źródła

- [Seeed Studio XIAO nRF54L15 Sense, czujniki na płytce](https://wiki.seeedstudio.com/xiao_nrf54l15_sense_built_in_sensor/)
- [XIAO nRF54L15, dokumentacja płytki w Zephyrze](https://docs.zephyrproject.org/latest/boards/seeed/xiao_nrf54l15/doc/index.html)
- [MicroPython dla XIAO nRF54L15](https://wiki.seeedstudio.com/xiao_nrf54l15_sense_micropython/)
- [Fermion MAX30102 (SEN0344), dokumentacja DFRobot](https://wiki.dfrobot.com/sen0344/)
- [Fermion ICG-20660L (SEN0443), dokumentacja DFRobot](https://wiki.dfrobot.com/Fermion_ICG_20660L_Accel_Gyro_6_Axis_IMU_Module_Breakout_SKU_SEN0443)
- [XIAO Logger HAT](https://www.seeedstudio.com/XIAO-LOG-p-6341.html)
- [Buzzer z generatorem 5 V 12 mm, karta katalogowa sprzedawcy](https://botland.store/buzzers-sound-generators/786-active-buzzer-with-generator-5v-12mm-tht-5904422366940.html)
- [Mini silnik wibracyjny MT35 3 V](https://botland.store/vibration-motors/4727-mini-vibration-motor-mt35-3v-5904422300166.html)
