# EPI — aplikacja monitorująca (prototyp)

Aplikacja towarzysząca opasce EPI, zbudowana na podstawie dokumentu koncepcyjnego
„Elektroniczny Detektor Napadów Padaczkowych" (wersja 0.1). Działa jako statyczna
strona — bez backendu. Konta i dane zostają w przeglądarce, nic nie jest
wysyłane na zewnątrz.

## Uruchomienie

Opublikowana wersja: https://kajetan98.github.io/web/aplikacja/ (strona projektu:
https://kajetan98.github.io/web/epi.html). Wersja na Androida:
https://github.com/Kajetan98/web/releases/latest/download/epi-android.apk

Katalog jest częścią strony SPACER i nie wymaga budowania. Lokalnie:

```
python3 -m http.server 8000
# http://localhost:8000/aplikacja/
```

Wersja językowa: `?lang=pl` lub `?lang=en`; wybór zapisuje się w ustawieniach.
Część funkcji (Web Bluetooth, service worker, instalacja jako PWA) wymaga HTTPS
albo `localhost`.

## Pliki

| Plik | Zawartość |
| --- | --- |
| `index.html` | Szkielet interfejsu: ekran logowania i rejestracji, cztery widoki, overlay alarmu i szczegółów zdarzenia |
| `app.js` | Całość logiki: źródła sygnału, analiza, automat detekcji, konta, magazyn danych, i18n |
| `app.css` | Style aplikacji (paleta zgodna ze stroną główną) |
| `manifest.webmanifest` | Instalacja jako aplikacja na telefonie |
| `sw.js` | Cache powłoki aplikacji; pliki z tej domeny network-first, cache jako zapas offline |

## Tor sygnału

Tor ruchowy czyta zdarzenia `devicemotion` (moduł wektora przyspieszenia razem
z grawitacją). Gdy urządzenie nie dostarcza próbek przez 3 sekundy — na przykład
na komputerze bez akcelerometru albo w karcie w tle — kanał wraca do wbudowanego
generatora, a interfejs pokazuje aktualne źródło.

Tor biometryczny łączy się przez Web Bluetooth z usługami Heart Rate (0x180D)
oraz Pulse Oximeter (0x1822, pomiar ciągły PLX z dekodowaniem SFLOAT
IEEE-11073). Bez czujnika tętno i SpO₂ pochodzą z generatora.

Analiza biegnie co 250 ms w oknie 4 sekund: od modułu przyspieszenia odejmowana
jest średnia okna (co usuwa składową grawitacyjną), z reszty liczona jest
amplituda RMS oraz częstotliwość dominująca z liczby przejść przez zero.

## Automat detekcji

Stany odpowiadają rozdziałowi 5 dokumentu:

- **IDLE** — analiza ruchu w tle, powolna aktualizacja linii bazowej tętna i SpO₂.
- **SUSPECT** — amplituda ≥ progu i częstotliwość w paśmie napadowym.
- **CONFIRMING** — wzorzec utrzymany przez zadany czas; startuje odliczanie
  z możliwością anulowania (przycisk w overlayu albo przycisk SOS). Okno można
  zwinąć do paska na górze — odliczanie biegnie dalej i nadal da się je
  anulować, a reszta aplikacji pozostaje dostępna.
- **ALARM** — po odliczeniu spełnione kryterium ruchowe i biometryczne.
- **ANULOWANO / odrzucone** — zapisywane do kalibracji progów.

Kryterium biometryczne wymaga spadku SpO₂ poniżej linii bazowej albo wzrostu
tętna, który jest jednocześnie odpowiednio gwałtowny (domyślnie ≥ 1,2 bpm/s).
Sam próg wartości tętna nie wystarcza, ponieważ wysiłek fizyczny również go
przekracza — rozstrzyga tempo narastania. Po każdym rozstrzygnięciu obowiązuje
45 sekund wyciszenia, żeby ta sama aktywność nie generowała serii zdarzeń.

Domyślne progi (amplituda 2,5 m/s², pasmo 2,5–5,5 Hz, utrzymanie 3 s, okno
potwierdzenia 15 s, wzrost tętna 35%, spadek SpO₂ 4 pp) pochodzą z wartości
orientacyjnych i wymagają kalibracji na danych rzeczywistych. Wszystkie są
edytowalne w zakładce Ustawienia.

## Konta

Aplikacja prowadzi konta lokalne: przy pierwszym wejściu proponuje rejestrację,
logowanie na istniejące konto albo pracę bez konta. Konto to wpis w
`epi.v1.accounts` z nazwą, opcjonalnym adresem e-mail, losową solą i skrótem
PIN-u (PBKDF2-SHA256, 120 000 iteracji) — sam PIN nigdy nie jest zapisywany.
Zalogowane konto zapamiętuje `epi.v1.session`.

Dane każdego konta trzymane są pod osobnym kluczem z sufiksem identyfikatora,
więc kilka osób może korzystać z jednej przeglądarki bez mieszania historii.
Praca bez konta to profil `guest`. Zapisy sprzed wprowadzenia kont trafiają do
profilu gościa, a przy zakładaniu pierwszego konta przechodzą na nie.

Nie jest to konto serwerowe: nic nie jest wysyłane ani synchronizowane między
urządzeniami, a PIN chroni wyłącznie przed zajrzeniem do danych na tym samym
urządzeniu. Na `file://` przeglądarka nie udostępnia Web Crypto — konto
powstaje wtedy bez PIN-u, o czym aplikacja informuje.

## Dane

Zdarzenia, kontakty i ustawienia trzymane są w `localStorage` pod kluczami
`epi.v1.events:<konto>`, `epi.v1.contacts:<konto>`, `epi.v1.settings:<konto>`. Każde zdarzenie zapisuje
przebieg sygnału w oknie −30 s / +60 s (4 Hz: amplituda ruchu, tętno, SpO₂),
szczytowe tętno, najniższe SpO₂ i czas trwania drgań. Historia trzyma 60
ostatnich zdarzeń.

Eksport: CSV zbiorczy, CSV pojedynczego przebiegu, JSON całej bazy oraz raport
do druku lub PDF. Powiadomienie kontaktów odbywa się przez odnośniki `sms:`
i `tel:` z przygotowaną treścią (czas, tętno, SpO₂, opcjonalnie lokalizacja);
wysyłkę potwierdza użytkownik w telefonie — aplikacja nie ma kanału serwerowego.

## Ograniczenia

- Prototyp badawczy na poziomie TRL 2→3. Nie jest wyrobem medycznym i nie
  zastępuje opieki lekarskiej.
- Pomiar PPG pochodzi z zewnętrznego czujnika BLE albo z generatora — telefon
  nie mierzy SpO₂.
- Przeglądarka w tle ogranicza działanie liczników i zdarzeń czujników, więc
  ciągłe monitorowanie wymaga aktywnego ekranu (aplikacja prosi o `wakeLock`).
- Web Bluetooth działa w Chrome i przeglądarkach opartych na Chromium; Safari
  i Firefox go nie udostępniają.
