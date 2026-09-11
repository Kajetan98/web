# Firmware EPI

Kod urządzenia dla Seeed XIAO nRF54L15 (Sense). Podział na warstwę logiki
niezależną od sprzętu i warstwę integracji z Zephyrem.

| Katalog | Zawartość | Stan |
| --- | --- | --- |
| `lib/` | dioda, bateria, przycisk, detektor | gotowe, pokryte testami |
| `tests/` | testy uruchamiane na komputerze | 492 sprawdzenia wersji na nRF, 111 tysięcy sprawdzeń porównawczych |
| `zephyr/` | devicetree, konfiguracja, czujniki, BLE | szkielet, nie był kompilowany przeciwko SDK |
| `arduino/` | szkic na Arduino Nano z modułem HC-06 | kompiluje się i konsoliduje dla ATmega328P |

## Testy

```
cd tests && make test
```

Potrzebny jest tylko kompilator C i `libm`. Testy sprawdzają priorytety stanów
diody, mapowanie poziomu naładowania na barwę i jej monotoniczność, wypełnienia
wzorców błysków, tablicę napięć ogniwa razem z filtrem i histerezą, gesty
przycisku wraz z filtrowaniem drgań styku oraz automat detekcji na sygnale
syntetycznym (przejście do alarmu, odrzucenie po ustaniu drgań, wyciszenie, brak
alarmu przy samym ruchu bez kryterium biometrycznego).

Drugi zestaw (`test_nano.c`) podaje obu implementacjom, na nRF54L15 i na
Arduino Nano, ten sam sygnał i porównuje wyniki: barwy diody dla każdego
poziomu naładowania, tryb i wypełnienie dla wszystkich kombinacji stanu
urządzenia, tablicę napięć ogniwa, zdarzenia przycisku oraz ciąg stanów
automatu. Bez tego dwie kopie tej samej logiki rozjeżdżają się po pierwszej
poprawce w jednej z nich.

## Moduły

`epi_led` rozstrzyga, który tryb obowiązuje przy danym stanie urządzenia,
zamienia poziom naładowania na barwę i generuje wypełnienia PWM dla zadanej
chwili. Priorytety i wzorce opisuje `hardware/README.md`.

`epi_battery` przelicza napięcie ogniwa na poziom naładowania z tablicy napięć
spoczynkowych, z filtrem wykładniczym i histerezą (wskazanie rośnie tylko przy
ładowaniu, więc impuls prądu z silnika albo buzzera nie zbija go w dół na stałe).
Tablica jest przybliżeniem i wymaga zastąpienia krzywą zmierzoną na docelowym
ogniwie.

`epi_button` dekoduje gesty: kliknięcie, przytrzymanie, bardzo długie
przytrzymanie, z filtrowaniem drgań styku 30 ms.

`epi_detector` to port automatu z `aplikacja/app.js`: te same stany, te same
cechy sygnału (RMS po usunięciu składowej stałej, częstotliwość z przejść przez
zero) i te same progi domyślne. Cechy są wystawione osobno
(`epi_det_features`), bo są wejściem klasyfikatora z drugiego etapu modelu.

## Budowanie na Arduino Nano

Katalog `arduino/EpiNano/` otwiera się w środowisku Arduino bez przygotowań.
Szczegóły połączeń, ustawienie modułu HC-06, protokół tekstowy i ograniczenia
tej wersji opisuje `arduino/README.md`.

## Budowanie na płytkę nRF54L15

```
west build -b xiao_nrf54l15/nrf54l15/cpuapp zephyr
west flash
```

Przed pierwszą kompilacją trzeba uzupełnić `zephyr/boards/xiao_nrf54l15_cpuapp.overlay`:
numery wyprowadzeń w blokach pinctrl dla PWM oraz węzeł pulsoksymetru. Otwarte
miejsca w kodzie są oznaczone komentarzem `TODO` i dotyczą odczytu napięcia
ogniwa z ADC oraz sterowania modułem MAX30102.

Nazwa opcji sterownika IMU (`CONFIG_LSM6DSO` czy osobna dla LSM6DS3TR-C) oraz
nazwa makra parametrów rozgłaszania BLE zależą od wersji SDK i wymagają
sprawdzenia przy pierwszej kompilacji.
