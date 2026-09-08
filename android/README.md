# EPI na Androida

Aplikacja EPI zapakowana jako APK. To ta sama aplikacja co w `aplikacja/` —
projekt Androida nie zawiera jej kopii, tylko kopiuje pliki z repozytorium
podczas budowania (`copySite` w `app/build.gradle`). Zmiana w aplikacji
webowej trafia więc do APK bez żadnej synchronizacji ręcznej.

Gotowy plik: https://github.com/Kajetan98/web/releases/latest/download/epi-android.apk

## Budowanie

Wymagane: JDK 17 i Android SDK (platforma 34).

```
cd android
./gradlew assembleDebug
# app/build/outputs/apk/debug/app-debug.apk
```

Repozytorium ma też workflow `.github/workflows/android.yml`, który buduje APK
na GitHubie: uruchamiany ręcznie („Run workflow" w zakładce Actions) albo przy
zmianie w `android/` lub `aplikacja/`. Plik trafia do artefaktów przebiegu.
Żeby opublikować wydanie, uruchom workflow ręcznie i wpisz tag w polu
`release_tag` (np. `v0.1.1`) — APK zostanie dołączony do wydania pod stałą
nazwą `epi-android.apk`. Ten sam efekt daje wypchnięcie tagu `v*`.

APK jest podpisany kluczem debugowym, więc instaluje się poza sklepem Play po
włączeniu zgody „Zainstaluj nieznane aplikacje". Do dystrybucji w sklepie
potrzebny jest własny keystore i wariant `assembleRelease`.

## Co działa, a co nie

WebView dostaje całą aplikację z `file:///android_asset/site/`, więc działa bez
internetu i bez serwera: monitorowanie, zapis zdarzeń w `localStorage`,
kontakty, eksport, wibracja i odczyt akcelerometru telefonu.

Web Bluetooth nie jest częścią WebView — połączenie z zewnętrznym pulsometrem
albo pulsoksymetrem BLE działa wyłącznie w wersji przeglądarkowej (Chrome).
W APK tor biometryczny pracuje na generatorze, co aplikacja pokazuje w polu
źródła sygnału.

Service worker jest pominięty w kopii (`exclude 'sw.js'`) — przy `file://`
i tak się nie rejestruje, a offline zapewnia samo spakowanie plików.

## Struktura

| Ścieżka | Zawartość |
| --- | --- |
| `app/src/main/java/pl/spacer/epi/MainActivity.java` | Powłoka WebView: localStorage, blokada wygaszania ekranu, obsługa przycisku Wstecz |
| `app/src/main/AndroidManifest.xml` | Uprawnienie do wibracji, aktywność startowa |
| `app/build.gradle` | Konfiguracja modułu i zadanie `copySite` |
| `app/src/main/res/` | Ikona, nazwa aplikacji, motyw w kolorach EPI |
