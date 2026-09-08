/**
 * EPI — aplikacja monitorująca (prototyp).
 *
 * Runs the fusion state machine from the EPI design document on whatever
 * signal sources the browser exposes: the device accelerometer for the motion
 * channel, a BLE heart-rate / pulse-oximeter sensor for the biometric channel,
 * and a built-in generator when neither is available. Events, contacts and
 * thresholds live in localStorage — there is no backend.
 */
(() => {
  'use strict';

  const VERSION = '0.1.0';
  const KEYS = {
    events: 'epi.v1.events',
    contacts: 'epi.v1.contacts',
    settings: 'epi.v1.settings'
  };

  const DEFAULTS = {
    lang: null,
    ampMin: 2.5,        // m/s^2 RMS of the gravity-removed acceleration
    freqMin: 2.5,       // Hz — tonic-clonic band
    freqMax: 5.5,
    hold: 3,            // s the motion pattern must persist before CONFIRMING
    window: 15,         // s of the confirmation countdown
    hrRise: 35,         // % above the resting baseline
    hrSlope: 1.2,       // bpm/s — separates a seizure from a warm-up
    spo2Drop: 4,        // percentage points below the resting baseline
    requireBoth: false,
    baselineHr: 68,
    baselineSpo2: 97,
    useMotion: true,
    sound: true,
    vibrate: true,
    notifications: false,
    geo: false
  };

  const THRESHOLD_KEYS = ['ampMin', 'freqMin', 'freqMax', 'hold', 'window', 'hrRise', 'spo2Drop', 'requireBoth'];

  /* ====================================================================== */
  /* i18n                                                                   */
  /* ====================================================================== */
  const STRINGS = {
    pl: {
      'doc.title': 'EPI — aplikacja monitorująca | SPACER',
      'header.tag': 'prototyp',
      'header.offline': 'Czujniki: symulacja',
      'header.exit': 'Wyjdź',
      'monitor.title': 'Monitorowanie',
      'monitor.start': 'Uruchom monitorowanie',
      'monitor.stop': 'Zatrzymaj',
      'monitor.connect': 'Połącz czujnik BLE',
      'monitor.connected': 'Rozłącz czujnik',
      'metric.hr': 'Tętno',
      'metric.amp': 'Amplituda ruchu',
      'metric.freq': 'Rytm drgań',
      'chart.ppg': 'Sygnał PPG',
      'chart.motion': 'Akcelerometr',
      'chart.motionNote': 'Czerwona linia to próg amplitudy, przy którym uruchamia się analiza rytmu drgań.',
      'fusion.title': 'Kryteria fuzji',
      'fusion.motion': 'Rytmiczne drgania w paśmie napadowym',
      'fusion.hr': 'Gwałtowny wzrost tętna',
      'fusion.spo2': 'Spadek SpO₂ względem linii bazowej',
      'fusion.note': 'Alarm wymaga kryterium ruchowego i co najmniej jednego biometrycznego — to odróżnia napad od wysiłku fizycznego.',
      'fusion.noteBoth': 'Alarm wymaga kryterium ruchowego oraz obu kryteriów biometrycznych naraz.',
      'demo.title': 'Tryb demonstracyjny',
      'demo.chip': 'bez sprzętu',
      'demo.note': 'Wstrzykuje sygnał do tego samego toru analizy, którym płyną dane z czujników — automat detekcji działa na nim bez żadnych ułatwień.',
      'demo.seizure': 'Symuluj napad',
      'demo.activity': 'Symuluj bieg',
      'demo.stop': 'Przerwij',
      'sos.note': 'przytrzymaj 1 s',
      'sos.cancel': 'ANULUJ',
      'sos.cancelNote': 'naciśnij, aby przerwać alarm',
      'state.off.name': 'Monitorowanie wstrzymane',
      'state.off.note': 'Uruchom detekcję, aby aplikacja zaczęła analizować ruch i tętno.',
      'state.idle.name': 'Monitorowanie aktywne',
      'state.idle.note': 'Stan IDLE — analiza ruchu w tle, PPG w trybie oszczędnym.',
      'state.suspect.name': 'Podejrzenie wzorca ruchowego',
      'state.suspect.note': 'Stan SUSPECT — wykryto rytmiczne drgania, PPG przeszedł na pełną częstotliwość.',
      'state.confirming.name': 'Potwierdzanie napadu',
      'state.confirming.note': 'Stan CONFIRMING — trwa sprawdzanie kryteriów biometrycznych.',
      'state.alarm.name': 'ALARM — napad wykryty',
      'state.alarm.note': 'Kontakty alarmowe czekają na wysyłkę wiadomości.',
      'tab.monitor': 'Monitoring',
      'tab.history': 'Historia',
      'tab.contacts': 'Kontakty',
      'tab.settings': 'Ustawienia',
      'history.title': 'Historia zdarzeń',
      'history.csv': 'Eksport CSV',
      'history.pdf': 'Raport PDF',
      'history.clear': 'Wyczyść',
      'history.empty': 'Brak zapisanych zdarzeń. Każde wykrycie, anulowanie i odrzucenie trafi tutaj razem z przebiegiem sygnału.',
      'history.confirmClear': 'Usunąć wszystkie zapisane zdarzenia? Tej operacji nie można cofnąć.',
      'contacts.title': 'Kontakty alarmowe',
      'contacts.lead': 'Osoby powiadamiane po potwierdzeniu napadu. Aplikacja przygotowuje treść wiadomości z czasem zdarzenia i parametrami — wysyłkę potwierdzasz w telefonie.',
      'contacts.empty': 'Nie dodano jeszcze żadnego kontaktu.',
      'contacts.add': 'Dodaj kontakt',
      'contacts.edit': 'Edytuj kontakt',
      'contacts.name': 'Imię i nazwisko',
      'contacts.relation': 'Relacja',
      'contacts.phone': 'Telefon',
      'contacts.notify': 'Powiadamiaj przy alarmie',
      'contacts.save': 'Zapisz kontakt',
      'contacts.cancelEdit': 'Anuluj edycję',
      'contacts.sms': 'SMS',
      'contacts.call': 'Zadzwoń',
      'contacts.editBtn': 'Edytuj',
      'contacts.delete': 'Usuń',
      'contacts.silent': 'bez powiadomień',
      'contacts.saved': 'Kontakt zapisany.',
      'contacts.removed': 'Kontakt usunięty.',
      'settings.title': 'Ustawienia',
      'settings.sources': 'Źródła danych',
      'settings.useMotion': 'Używaj akcelerometru urządzenia',
      'settings.motionNote': 'Na telefonie aplikacja analizuje realny sygnał ruchu. Na komputerze bez akcelerometru automatycznie działa symulator.',
      'settings.motionPerm': 'Poproś o dostęp do czujnika ruchu',
      'settings.ble': 'Połącz pulsometr / pulsoksymetr BLE',
      'settings.thresholds': 'Progi detekcji',
      'settings.amp': 'Minimalna amplituda drgań',
      'settings.freqMin': 'Dolna granica pasma',
      'settings.freqMax': 'Górna granica pasma',
      'settings.hold': 'Czas utrzymania wzorca (SUSPECT)',
      'settings.window': 'Okno potwierdzenia',
      'settings.hr': 'Wzrost tętna ponad linię bazową',
      'settings.spo2': 'Spadek SpO₂ poniżej linii bazowej',
      'settings.both': 'Wymagaj obu kryteriów biometrycznych naraz',
      'settings.reset': 'Przywróć wartości domyślne',
      'settings.baseline': 'Linia bazowa',
      'settings.baselineNote': 'Aktualizowana automatycznie w stanie spoczynku; wartości można też ustawić ręcznie.',
      'settings.baseHr': 'Tętno spoczynkowe (bpm)',
      'settings.baseSpo2': 'SpO₂ spoczynkowe (%)',
      'settings.alarm': 'Alarm',
      'settings.sound': 'Sygnał dźwiękowy',
      'settings.vibrate': 'Wibracja',
      'settings.notif': 'Powiadomienia systemowe',
      'settings.geo': 'Dołącz lokalizację do wiadomości',
      'settings.data': 'Dane',
      'settings.exportJson': 'Eksport JSON',
      'settings.wipe': 'Usuń wszystkie dane',
      'settings.lang': 'Język',
      'settings.disclaimer': 'Prototyp badawczy na poziomie TRL 2→3. Nie jest wyrobem medycznym, nie zastępuje opieki lekarskiej i nie może być jedynym zabezpieczeniem osoby z padaczką. Progi detekcji wymagają kalibracji na danych rzeczywistych.',
      'settings.confirmWipe': 'Usunąć zdarzenia, kontakty i ustawienia z tego urządzenia?',
      'settings.storage': 'Zdarzenia: {events} · kontakty: {contacts} · zajęte miejsce: {size} kB',
      'alarm.countNote': 's do powiadomienia',
      'alarm.cancel': 'To fałszywy alarm — anuluj',
      'alarm.close': 'Zamknij',
      'alarm.confirming': 'Potwierdzanie napadu',
      'alarm.confirmingTitle': 'Wykryto wzorzec napadu toniczno-klonicznego',
      'alarm.confirmingNote': 'Po odliczeniu aplikacja przygotuje wiadomości do kontaktów alarmowych i zapisze przebieg zdarzenia.',
      'alarm.manual': 'Alarm ręczny (SOS)',
      'alarm.manualTitle': 'Wezwanie pomocy uruchomione przyciskiem SOS',
      'alarm.sent': 'Alarm potwierdzony',
      'alarm.sentTitle': 'Powiadom kontakty alarmowe',
      'alarm.sentNote': 'Przebieg zdarzenia zapisany w historii (−30 s / +60 s). Wysyłka SMS wymaga potwierdzenia w telefonie.',
      'alarm.noContacts': 'Nie dodano kontaktów alarmowych — uzupełnij je w zakładce Kontakty.',
      'alarm.cancelled': 'Alarm anulowany',
      'alarm.cancelledTitle': 'Zdarzenie zapisane jako fałszywy alarm',
      'alarm.cancelledNote': 'Zapis posłuży do kalibracji progów detekcji.',
      'alarm.rejectedTitle': 'Wzorzec odrzucony przez fuzję danych',
      'alarm.rejected': 'Odrzucono',
      'alarm.rejectedNote': 'Ruch spełnił kryterium, ale tętno i SpO₂ nie potwierdziły napadu. Zdarzenie zapisano do kalibracji.',
      'alarm.trigger.freq': 'Rytm drgań',
      'alarm.trigger.amp': 'Amplituda',
      'alarm.trigger.hr': 'Tętno',
      'alarm.trigger.spo2': 'SpO₂',
      'alarm.notifyBtn': 'SMS',
      'alarm.callBtn': 'Telefon',
      'sms.body': 'ALARM EPI: wykryto napad padaczkowy o {time}. Tętno {hr} bpm, SpO2 {spo2}%. Wiadomość wysłana z prototypu aplikacji EPI.',
      'sms.location': ' Lokalizacja: {url}',
      'sheet.window': 'Zapis zdarzenia (−30 s / +60 s)',
      'sheet.legendAcc': 'amplituda ruchu',
      'sheet.legendHr': 'tętno',
      'sheet.csv': 'Eksport przebiegu CSV',
      'sheet.delete': 'Usuń zdarzenie',
      'sheet.duration': 'Czas drgań',
      'sheet.peakHr': 'Szczytowe tętno',
      'sheet.minSpo2': 'Najniższe SpO₂',
      'sheet.freq': 'Rytm drgań',
      'sheet.amp': 'Amplituda',
      'sheet.source': 'Źródło sygnału',
      'sheet.notified': 'Powiadomieni',
      'sheet.none': 'brak',
      'event.tonic': 'Napad toniczno-kloniczny',
      'event.manual': 'Alarm ręczny (SOS)',
      'event.rejected': 'Wzorzec odrzucony przez fuzję',
      'event.notified': 'Alarm',
      'event.cancelled': 'Fałszywy',
      'event.rejectedTag': 'Odrzucony',
      'event.deleted': 'Zdarzenie usunięte.',
      'source.motion': 'akcelerometr',
      'source.sim': 'symulacja',
      'source.ble': 'BLE',
      'toast.started': 'Monitorowanie uruchomione.',
      'toast.stopped': 'Monitorowanie zatrzymane.',
      'toast.motionOn': 'Akcelerometr urządzenia podłączony do toru ruchu.',
      'toast.motionDenied': 'Brak zgody na dostęp do czujnika ruchu — pracuję na symulatorze.',
      'toast.motionMissing': 'To urządzenie nie udostępnia akcelerometru — pracuję na symulatorze.',
      'toast.bleMissing': 'Ta przeglądarka nie obsługuje Web Bluetooth. Użyj Chrome na Androidzie lub desktopie.',
      'toast.bleConnected': 'Czujnik BLE podłączony: {name}.',
      'toast.bleLost': 'Utracono połączenie z czujnikiem BLE — wracam do symulacji.',
      'toast.bleFailed': 'Nie udało się połączyć z czujnikiem BLE.',
      'toast.simSeizure': 'Symulacja napadu uruchomiona — obserwuj kryteria fuzji.',
      'toast.simActivity': 'Symulacja biegu — ruch spełni kryterium, biometria nie.',
      'toast.simStop': 'Symulacja przerwana.',
      'toast.rejected': 'Wzorzec odrzucony: brak potwierdzenia biometrycznego.',
      'toast.exported': 'Plik przygotowany do pobrania.',
      'toast.noEvents': 'Brak zdarzeń do eksportu.',
      'toast.wiped': 'Dane usunięte.',
      'toast.notifBlocked': 'Powiadomienia systemowe zablokowane w przeglądarce.',
      'toast.storageFull': 'Brak miejsca w pamięci przeglądarki — usuń starsze zdarzenia.',
      'notif.title': 'EPI — wykryto napad',
      'notif.body': 'Potwierdzony napad o {time}. Otwórz aplikację i powiadom kontakty.',
      'print.title': 'EPI — raport zdarzeń',
      'print.sub': 'Wygenerowano {date} · prototyp badawczy, nie jest wyrobem medycznym',
      'print.when': 'Data i godzina',
      'print.kind': 'Zdarzenie',
      'print.outcome': 'Wynik',
      'print.duration': 'Czas drgań',
      'print.hr': 'Tętno szcz.',
      'print.spo2': 'SpO₂ min.',
      'print.foot': 'Dane pochodzą z prototypu EPI i wymagają interpretacji klinicznej.',
      'ble.unsupported': 'Web Bluetooth nie jest dostępny w tej przeglądarce — tor biometryczny pracuje na symulatorze.',
      'ble.supported': 'Obsługiwane usługi: Heart Rate (0x180D) oraz Pulse Oximeter (0x1822).',
      'motion.granted': 'Akcelerometr aktywny — analizowany jest realny sygnał ruchu.',
      'motion.unavailable': 'Akcelerometr niedostępny w tym urządzeniu lub przeglądarce.'
    },
    en: {
      'doc.title': 'EPI — monitoring app | SPACER',
      'header.tag': 'prototype',
      'header.offline': 'Sensors: simulated',
      'header.exit': 'Exit',
      'monitor.title': 'Monitoring',
      'monitor.start': 'Start monitoring',
      'monitor.stop': 'Stop',
      'monitor.connect': 'Connect BLE sensor',
      'monitor.connected': 'Disconnect sensor',
      'metric.hr': 'Heart rate',
      'metric.amp': 'Motion amplitude',
      'metric.freq': 'Shaking rhythm',
      'chart.ppg': 'PPG signal',
      'chart.motion': 'Accelerometer',
      'chart.motionNote': 'The red line marks the amplitude threshold that starts the rhythm analysis.',
      'fusion.title': 'Fusion criteria',
      'fusion.motion': 'Rhythmic shaking inside the seizure band',
      'fusion.hr': 'Abrupt heart-rate rise',
      'fusion.spo2': 'SpO₂ drop below baseline',
      'fusion.note': 'An alarm needs the motion criterion plus at least one biometric criterion — that is what separates a seizure from exercise.',
      'fusion.noteBoth': 'An alarm needs the motion criterion plus both biometric criteria at once.',
      'demo.title': 'Demo mode',
      'demo.chip': 'no hardware',
      'demo.note': 'Injects a signal into the same analysis path the sensors feed — the detector runs on it with no shortcuts.',
      'demo.seizure': 'Simulate seizure',
      'demo.activity': 'Simulate running',
      'demo.stop': 'Stop signal',
      'sos.note': 'hold for 1 s',
      'sos.cancel': 'CANCEL',
      'sos.cancelNote': 'press to stop the alarm',
      'state.off.name': 'Monitoring paused',
      'state.off.note': 'Start the detector to analyse motion and heart rate.',
      'state.idle.name': 'Monitoring active',
      'state.idle.note': 'IDLE — motion analysed in the background, PPG in low-power mode.',
      'state.suspect.name': 'Motion pattern suspected',
      'state.suspect.note': 'SUSPECT — rhythmic shaking detected, PPG switched to full sampling rate.',
      'state.confirming.name': 'Confirming seizure',
      'state.confirming.note': 'CONFIRMING — checking the biometric criteria.',
      'state.alarm.name': 'ALARM — seizure detected',
      'state.alarm.note': 'Emergency contacts are waiting for the message to be sent.',
      'tab.monitor': 'Monitor',
      'tab.history': 'History',
      'tab.contacts': 'Contacts',
      'tab.settings': 'Settings',
      'history.title': 'Event history',
      'history.csv': 'Export CSV',
      'history.pdf': 'PDF report',
      'history.clear': 'Clear',
      'history.empty': 'No events stored yet. Every detection, cancellation and rejection lands here together with the signal trace.',
      'history.confirmClear': 'Delete every stored event? This cannot be undone.',
      'contacts.title': 'Emergency contacts',
      'contacts.lead': 'People notified once a seizure is confirmed. The app prepares the message with the event time and vitals — you confirm sending on the phone.',
      'contacts.empty': 'No contacts added yet.',
      'contacts.add': 'Add contact',
      'contacts.edit': 'Edit contact',
      'contacts.name': 'Full name',
      'contacts.relation': 'Relationship',
      'contacts.phone': 'Phone',
      'contacts.notify': 'Notify on alarm',
      'contacts.save': 'Save contact',
      'contacts.cancelEdit': 'Cancel editing',
      'contacts.sms': 'SMS',
      'contacts.call': 'Call',
      'contacts.editBtn': 'Edit',
      'contacts.delete': 'Delete',
      'contacts.silent': 'not notified',
      'contacts.saved': 'Contact saved.',
      'contacts.removed': 'Contact removed.',
      'settings.title': 'Settings',
      'settings.sources': 'Signal sources',
      'settings.useMotion': 'Use the device accelerometer',
      'settings.motionNote': 'On a phone the app analyses a real motion signal. On a desktop without an accelerometer it falls back to the simulator.',
      'settings.motionPerm': 'Request motion sensor access',
      'settings.ble': 'Connect a BLE heart-rate / pulse oximeter',
      'settings.thresholds': 'Detection thresholds',
      'settings.amp': 'Minimum shaking amplitude',
      'settings.freqMin': 'Lower band edge',
      'settings.freqMax': 'Upper band edge',
      'settings.hold': 'Pattern hold time (SUSPECT)',
      'settings.window': 'Confirmation window',
      'settings.hr': 'Heart-rate rise above baseline',
      'settings.spo2': 'SpO₂ drop below baseline',
      'settings.both': 'Require both biometric criteria at once',
      'settings.reset': 'Restore defaults',
      'settings.baseline': 'Baseline',
      'settings.baselineNote': 'Updated automatically at rest; the values can also be set by hand.',
      'settings.baseHr': 'Resting heart rate (bpm)',
      'settings.baseSpo2': 'Resting SpO₂ (%)',
      'settings.alarm': 'Alarm',
      'settings.sound': 'Audible alert',
      'settings.vibrate': 'Vibration',
      'settings.notif': 'System notifications',
      'settings.geo': 'Attach location to the message',
      'settings.data': 'Data',
      'settings.exportJson': 'Export JSON',
      'settings.wipe': 'Delete all data',
      'settings.lang': 'Language',
      'settings.disclaimer': 'Research prototype at TRL 2→3. It is not a medical device, does not replace medical care and must not be the only safeguard for a person with epilepsy. The detection thresholds require calibration on real data.',
      'settings.confirmWipe': 'Delete events, contacts and settings from this device?',
      'settings.storage': 'Events: {events} · contacts: {contacts} · storage used: {size} kB',
      'alarm.countNote': 's until notification',
      'alarm.cancel': 'False alarm — cancel',
      'alarm.close': 'Close',
      'alarm.confirming': 'Confirming seizure',
      'alarm.confirmingTitle': 'Tonic-clonic seizure pattern detected',
      'alarm.confirmingNote': 'When the countdown ends the app prepares messages to the emergency contacts and stores the event trace.',
      'alarm.manual': 'Manual alarm (SOS)',
      'alarm.manualTitle': 'Help requested with the SOS button',
      'alarm.sent': 'Alarm confirmed',
      'alarm.sentTitle': 'Notify the emergency contacts',
      'alarm.sentNote': 'Event trace stored in history (−30 s / +60 s). Sending the SMS needs confirmation on the phone.',
      'alarm.noContacts': 'No emergency contacts yet — add them in the Contacts tab.',
      'alarm.cancelled': 'Alarm cancelled',
      'alarm.cancelledTitle': 'Event stored as a false alarm',
      'alarm.cancelledNote': 'The recording feeds threshold calibration.',
      'alarm.rejectedTitle': 'Pattern rejected by data fusion',
      'alarm.rejected': 'Rejected',
      'alarm.rejectedNote': 'The motion criterion held, but heart rate and SpO₂ did not confirm a seizure. The event was stored for calibration.',
      'alarm.trigger.freq': 'Shaking rhythm',
      'alarm.trigger.amp': 'Amplitude',
      'alarm.trigger.hr': 'Heart rate',
      'alarm.trigger.spo2': 'SpO₂',
      'alarm.notifyBtn': 'SMS',
      'alarm.callBtn': 'Call',
      'sms.body': 'EPI ALARM: seizure detected at {time}. Heart rate {hr} bpm, SpO2 {spo2}%. Sent from the EPI app prototype.',
      'sms.location': ' Location: {url}',
      'sheet.window': 'Event recording (−30 s / +60 s)',
      'sheet.legendAcc': 'motion amplitude',
      'sheet.legendHr': 'heart rate',
      'sheet.csv': 'Export trace CSV',
      'sheet.delete': 'Delete event',
      'sheet.duration': 'Shaking duration',
      'sheet.peakHr': 'Peak heart rate',
      'sheet.minSpo2': 'Lowest SpO₂',
      'sheet.freq': 'Shaking rhythm',
      'sheet.amp': 'Amplitude',
      'sheet.source': 'Signal source',
      'sheet.notified': 'Notified',
      'sheet.none': 'none',
      'event.tonic': 'Tonic-clonic seizure',
      'event.manual': 'Manual alarm (SOS)',
      'event.rejected': 'Pattern rejected by fusion',
      'event.notified': 'Alarm',
      'event.cancelled': 'False alarm',
      'event.rejectedTag': 'Rejected',
      'event.deleted': 'Event deleted.',
      'source.motion': 'accelerometer',
      'source.sim': 'simulation',
      'source.ble': 'BLE',
      'toast.started': 'Monitoring started.',
      'toast.stopped': 'Monitoring stopped.',
      'toast.motionOn': 'Device accelerometer wired into the motion channel.',
      'toast.motionDenied': 'Motion sensor access denied — running on the simulator.',
      'toast.motionMissing': 'This device exposes no accelerometer — running on the simulator.',
      'toast.bleMissing': 'This browser has no Web Bluetooth. Use Chrome on Android or desktop.',
      'toast.bleConnected': 'BLE sensor connected: {name}.',
      'toast.bleLost': 'Lost the BLE sensor — falling back to simulation.',
      'toast.bleFailed': 'Could not connect to the BLE sensor.',
      'toast.simSeizure': 'Seizure simulation running — watch the fusion criteria.',
      'toast.simActivity': 'Running simulation — motion will qualify, biometrics will not.',
      'toast.simStop': 'Simulation stopped.',
      'toast.rejected': 'Pattern rejected: no biometric confirmation.',
      'toast.exported': 'File ready to download.',
      'toast.noEvents': 'No events to export.',
      'toast.wiped': 'Data deleted.',
      'toast.notifBlocked': 'System notifications are blocked in the browser.',
      'toast.storageFull': 'Browser storage is full — delete older events.',
      'notif.title': 'EPI — seizure detected',
      'notif.body': 'Seizure confirmed at {time}. Open the app and notify your contacts.',
      'print.title': 'EPI — event report',
      'print.sub': 'Generated {date} · research prototype, not a medical device',
      'print.when': 'Date and time',
      'print.kind': 'Event',
      'print.outcome': 'Outcome',
      'print.duration': 'Shaking',
      'print.hr': 'Peak HR',
      'print.spo2': 'Min SpO₂',
      'print.foot': 'The data comes from the EPI prototype and needs clinical interpretation.',
      'ble.unsupported': 'Web Bluetooth is unavailable in this browser — the biometric channel runs on the simulator.',
      'ble.supported': 'Supported services: Heart Rate (0x180D) and Pulse Oximeter (0x1822).',
      'motion.granted': 'Accelerometer active — a real motion signal is being analysed.',
      'motion.unavailable': 'No accelerometer available on this device or browser.'
    }
  };

  let lang = 'pl';
  const t = (key, vars) => {
    let s = (STRINGS[lang] && STRINGS[lang][key]) || STRINGS.pl[key] || key;
    if (vars) for (const k in vars) s = s.split('{' + k + '}').join(vars[k]);
    return s;
  };

  /* ====================================================================== */
  /* Storage                                                                */
  /* ====================================================================== */
  const store = {
    read(key, fallback) {
      try {
        const raw = localStorage.getItem(key);
        return raw ? JSON.parse(raw) : fallback;
      } catch (err) {
        return fallback;
      }
    },
    write(key, value) {
      try {
        localStorage.setItem(key, JSON.stringify(value));
        return true;
      } catch (err) {
        toast(t('toast.storageFull'), 'bad');
        return false;
      }
    },
    remove(key) {
      try { localStorage.removeItem(key); } catch (err) { /* private mode */ }
    },
    size() {
      let bytes = 0;
      for (const key of Object.values(KEYS)) {
        try { bytes += (localStorage.getItem(key) || '').length; } catch (err) { /* ignore */ }
      }
      return Math.round(bytes / 1024);
    }
  };

  let settings = Object.assign({}, DEFAULTS, store.read(KEYS.settings, {}));
  let events = store.read(KEYS.events, []);
  let contacts = store.read(KEYS.contacts, []);

  const saveSettings = () => store.write(KEYS.settings, settings);
  const saveEvents = () => store.write(KEYS.events, events);
  const saveContacts = () => store.write(KEYS.contacts, contacts);

  /* ====================================================================== */
  /* DOM helpers                                                            */
  /* ====================================================================== */
  const $ = (id) => document.getElementById(id);
  const el = {};
  [
    'linkPill', 'linkLabel', 'batteryPill', 'exitLink', 'appMain',
    'stateCard', 'stateName', 'stateNote', 'btnRun', 'btnConnect',
    'mHr', 'mSpo2', 'mAmp', 'mFreq', 'mHrBase', 'mSpo2Base', 'mAmpThr', 'mFreqThr',
    'ppgChart', 'motionChart', 'ppgChip', 'motionChip', 'fusionNote',
    'critMotion', 'critHr', 'critSpo2', 'criteria',
    'btnSimSeizure', 'btnSimActivity', 'btnSimStop',
    'btnSos', 'sosLabel', 'sosNote',
    'eventList', 'historyEmpty', 'historyCount', 'btnExportCsv', 'btnExportPdf', 'btnClearHistory',
    'contactList', 'contactsEmpty', 'contactsCount', 'contactForm', 'cName', 'cRelation', 'cPhone', 'cNotify',
    'contactSubmit', 'contactCancel',
    'alarmOverlay', 'alarmState', 'alarmTitle', 'alarmCount', 'alarmCountWrap', 'alarmTriggers',
    'alarmContacts', 'alarmNote', 'btnAlarmCancel', 'btnAlarmClose',
    'eventSheet', 'sheetClose', 'sheetWhen', 'sheetTitle', 'sheetFacts', 'sheetChart',
    'btnSheetCsv', 'btnSheetDelete',
    'toasts', 'tabbar', 'printReport', 'appVersion', 'storageInfo',
    'setMotion', 'btnMotionPerm', 'motionSupport', 'bleSupport', 'btnBle',
    'setAmp', 'setFreqMin', 'setFreqMax', 'setHold', 'setWindow', 'setHr', 'setSpo2', 'setBoth',
    'valAmp', 'valFreqMin', 'valFreqMax', 'valHold', 'valWindow', 'valHr', 'valSpo2',
    'btnResetThresholds', 'setBaseHr', 'setBaseSpo2',
    'setSound', 'setVibrate', 'setNotify', 'setGeo',
    'btnExportJson', 'btnWipe', 'langPl', 'langEn'
  ].forEach((id) => { el[id] = $(id); });

  function toast(message, kind) {
    const node = document.createElement('div');
    node.className = 'toast' + (kind ? ' ' + kind : '');
    node.textContent = message;
    el.toasts.appendChild(node);
    setTimeout(() => node.remove(), 5200);
  }

  /* ====================================================================== */
  /* Signal buffers and DSP                                                 */
  /* ====================================================================== */
  const ANALYSIS_WINDOW = 4;        // s
  const RECORD_HZ = 4;
  const RECORD_SPAN = 150;          // s kept in the rolling recorder
  const PRE_ROLL = 30;              // s stored before an event
  const POST_ROLL = 60;             // s stored after an alarm
  const POST_ROLL_SHORT = 15;       // s stored after a cancellation or rejection

  const motionSamples = [];         // {t, mag} at the source's native rate
  const hrHistory = [];             // {t, hr} at ~1 Hz
  const recorder = [];              // {t, acc, hr, spo2} at RECORD_HZ
  const ppgTrace = new Array(220).fill(0);

  let vitals = { hr: DEFAULTS.baselineHr, spo2: DEFAULTS.baselineSpo2 };
  let analysis = { amp: 0, freq: 0, motionPass: false, hrPass: false, spo2Pass: false, hrSlope: 0 };

  const now = () => Date.now();

  function pushMotion(t, mag) {
    motionSamples.push({ t, mag });
    const cutoff = t - (ANALYSIS_WINDOW + 2) * 1000;
    while (motionSamples.length && motionSamples[0].t < cutoff) motionSamples.shift();
  }

  /** RMS amplitude and dominant frequency of the gravity-removed magnitude. */
  function analyseMotion() {
    const t = now();
    const from = t - ANALYSIS_WINDOW * 1000;
    const win = motionSamples.filter((s) => s.t >= from);
    if (win.length < 16) return { amp: 0, freq: 0 };

    let sum = 0;
    for (const s of win) sum += s.mag;
    const mean = sum / win.length;

    let sq = 0;
    let crossings = 0;
    let prev = win[0].mag - mean;
    for (let i = 1; i < win.length; i++) {
      const dev = win[i].mag - mean;
      sq += dev * dev;
      if ((prev < 0 && dev >= 0) || (prev > 0 && dev <= 0)) crossings++;
      prev = dev;
    }
    const span = (win[win.length - 1].t - win[0].t) / 1000 || ANALYSIS_WINDOW;
    return {
      amp: Math.sqrt(sq / (win.length - 1)),
      freq: crossings / (2 * span)
    };
  }

  function hrSlope() {
    const t = now();
    const recent = hrHistory.filter((s) => s.t >= t - 20000);
    if (recent.length < 4) return 0;
    const first = recent[0];
    const last = recent[recent.length - 1];
    const dt = (last.t - first.t) / 1000;
    return dt > 3 ? (last.hr - first.hr) / dt : 0;
  }

  /* ====================================================================== */
  /* Signal sources                                                         */
  /* ====================================================================== */
  const sources = {
    motion: 'sim',   // 'sim' | 'device'
    vitals: 'sim'    // 'sim' | 'ble'
  };

  // --- simulation --------------------------------------------------------
  const sim = {
    mode: 'rest',    // 'rest' | 'seizure' | 'activity'
    since: 0,
    phase: 0,
    hr: DEFAULTS.baselineHr,
    spo2: DEFAULTS.baselineSpo2
  };

  function setSimMode(mode) {
    sim.mode = mode;
    sim.since = now();
  }

  /** One simulator step at SIM_HZ; feeds the same pipeline as real sensors. */
  function simStep(dt) {
    const elapsed = (now() - sim.since) / 1000;
    let mag = 9.81 + (Math.random() - 0.5) * 0.16;
    let targetHr = settings.baselineHr;
    let targetSpo2 = settings.baselineSpo2;
    let hrRate = 0.4;

    if (sim.mode === 'seizure') {
      sim.phase += 2 * Math.PI * 3.4 * dt;
      const ramp = Math.min(1, elapsed / 2);
      mag += Math.sin(sim.phase) * 4.4 * ramp + (Math.random() - 0.5) * 0.6;
      targetHr = 152;
      targetSpo2 = 90;
      hrRate = 3.2;
    } else if (sim.mode === 'activity') {
      sim.phase += 2 * Math.PI * 2.8 * dt;
      const ramp = Math.min(1, elapsed / 3);
      mag += Math.sin(sim.phase) * 4.8 * ramp + (Math.random() - 0.5) * 0.9;
      targetHr = 148;
      targetSpo2 = settings.baselineSpo2;
      hrRate = 0.45;   // a warm-up climbs slowly; a seizure does not
    }

    // A silent device sensor (backgrounded tab, revoked permission) hands the
    // channel back to the simulator rather than leaving the detector blind.
    if (sources.motion === 'device' && now() - lastDeviceMotionAt > 3000) {
      sources.motion = 'sim';
      updateSourceChips();
      reportMotionStatus();
    }
    if (sources.motion !== 'device') pushMotion(now(), mag);

    if (sources.vitals !== 'ble') {
      const stepHr = hrRate * dt * (targetHr > sim.hr ? 1 : -1.4);
      sim.hr += stepHr;
      if ((targetHr - sim.hr) * stepHr < 0) sim.hr = targetHr;
      sim.spo2 += (targetSpo2 - sim.spo2) * Math.min(1, dt * 0.35);
      vitals.hr = Math.round(sim.hr + (Math.random() - 0.5) * 1.6);
      vitals.spo2 = Math.round((sim.spo2 + (Math.random() - 0.5) * 0.5) * 10) / 10;
    }
  }

  // --- device accelerometer ---------------------------------------------
  let motionListening = false;
  let lastDeviceMotionAt = 0;

  function onDeviceMotion(event) {
    const a = event.accelerationIncludingGravity || event.acceleration;
    if (!a || a.x === null) return;
    const mag = Math.sqrt((a.x || 0) ** 2 + (a.y || 0) ** 2 + (a.z || 0) ** 2);
    lastDeviceMotionAt = now();
    if (sources.motion !== 'device') {
      sources.motion = 'device';
      updateSourceChips();
    }
    pushMotion(lastDeviceMotionAt, mag);
  }

  function reportMotionStatus() {
    el.motionSupport.textContent = sources.motion === 'device'
      ? t('motion.granted')
      : (typeof DeviceMotionEvent === 'undefined' ? t('motion.unavailable') : t('settings.motionNote'));
  }

  async function enableMotion() {
    if (typeof DeviceMotionEvent === 'undefined') {
      toast(t('toast.motionMissing'), 'warn');
      return false;
    }
    if (typeof DeviceMotionEvent.requestPermission === 'function') {
      try {
        const granted = await DeviceMotionEvent.requestPermission();
        if (granted !== 'granted') {
          toast(t('toast.motionDenied'), 'warn');
          return false;
        }
      } catch (err) {
        toast(t('toast.motionDenied'), 'warn');
        return false;
      }
    }
    if (!motionListening) {
      window.addEventListener('devicemotion', onDeviceMotion);
      motionListening = true;
    }
    // The permission can be granted on a machine that never emits readings,
    // so confirm the channel only once real samples arrive.
    setTimeout(() => {
      if (sources.motion === 'device') toast(t('toast.motionOn'));
      else toast(t('toast.motionMissing'), 'warn');
      reportMotionStatus();
    }, 1500);
    return true;
  }

  function disableMotion() {
    if (motionListening) {
      window.removeEventListener('devicemotion', onDeviceMotion);
      motionListening = false;
    }
    sources.motion = 'sim';
    updateSourceChips();
  }

  // --- BLE heart rate / pulse oximeter ----------------------------------
  let bleDevice = null;

  /** IEEE-11073 16-bit SFLOAT, used by the pulse oximeter service. */
  function readSfloat(view, offset) {
    const raw = view.getUint16(offset, true);
    let mantissa = raw & 0x0fff;
    let exponent = raw >> 12;
    if (exponent >= 0x0008) exponent = -((0x000f + 1) - exponent);
    if (mantissa >= 0x0800) mantissa = -((0x0fff + 1) - mantissa);
    return mantissa * Math.pow(10, exponent);
  }

  function onHeartRate(event) {
    const view = event.target.value;
    const flags = view.getUint8(0);
    const hr = (flags & 0x01) ? view.getUint16(1, true) : view.getUint8(1);
    if (hr > 20 && hr < 250) {
      vitals.hr = hr;
      sources.vitals = 'ble';
    }
  }

  function onPulseOximeter(event) {
    const view = event.target.value;
    const spo2 = readSfloat(view, 1);
    const pr = readSfloat(view, 3);
    if (spo2 > 50 && spo2 <= 100) vitals.spo2 = Math.round(spo2 * 10) / 10;
    if (pr > 20 && pr < 250) vitals.hr = Math.round(pr);
    sources.vitals = 'ble';
  }

  async function connectBle() {
    if (!navigator.bluetooth) {
      toast(t('toast.bleMissing'), 'warn');
      return;
    }
    if (bleDevice && bleDevice.gatt.connected) {
      bleDevice.gatt.disconnect();
      return;
    }
    try {
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ services: ['heart_rate'] }, { services: ['pulse_oximeter'] }],
        optionalServices: ['heart_rate', 'pulse_oximeter', 'battery_service']
      });
      const server = await device.gatt.connect();
      bleDevice = device;
      device.addEventListener('gattserverdisconnected', () => {
        sources.vitals = 'sim';
        bleDevice = null;
        updateSourceChips();
        toast(t('toast.bleLost'), 'warn');
      });

      let hooked = false;
      try {
        const hrService = await server.getPrimaryService('heart_rate');
        const ch = await hrService.getCharacteristic('heart_rate_measurement');
        await ch.startNotifications();
        ch.addEventListener('characteristicvaluechanged', onHeartRate);
        hooked = true;
      } catch (err) { /* device without the HR service */ }

      try {
        const plxService = await server.getPrimaryService('pulse_oximeter');
        const ch = await plxService.getCharacteristic('plx_continuous_measurement');
        await ch.startNotifications();
        ch.addEventListener('characteristicvaluechanged', onPulseOximeter);
        hooked = true;
      } catch (err) { /* device without the PLX service */ }

      if (!hooked) {
        toast(t('toast.bleFailed'), 'bad');
        device.gatt.disconnect();
        return;
      }
      sources.vitals = 'ble';
      updateSourceChips();
      toast(t('toast.bleConnected', { name: device.name || 'BLE' }));
    } catch (err) {
      if (err && err.name === 'NotFoundError') return;   // user dismissed the chooser
      toast(t('toast.bleFailed'), 'bad');
    }
  }

  /* ====================================================================== */
  /* Detector                                                               */
  /* ====================================================================== */
  const COOLDOWN = 45;      // s of silence after a resolved event

  const detector = {
    running: false,
    state: 'off',           // off | idle | suspect | confirming | alarm
    patternSince: 0,
    patternLost: 0,
    confirmUntil: 0,
    cooldownUntil: 0,
    current: null           // event being built
  };

  function evaluateCriteria() {
    const m = analyseMotion();
    analysis.amp = m.amp;
    analysis.freq = m.freq;
    analysis.motionPass = m.amp >= settings.ampMin && m.freq >= settings.freqMin && m.freq <= settings.freqMax;
    analysis.hrSlope = hrSlope();
    analysis.hrPass = vitals.hr >= settings.baselineHr * (1 + settings.hrRise / 100)
      && analysis.hrSlope >= settings.hrSlope;
    analysis.spo2Pass = vitals.spo2 <= settings.baselineSpo2 - settings.spo2Drop;
  }

  function bioConfirmed() {
    return settings.requireBoth
      ? (analysis.hrPass && analysis.spo2Pass)
      : (analysis.hrPass || analysis.spo2Pass);
  }

  function setState(state) {
    if (detector.state === state) return;
    detector.state = state;
    el.stateCard.dataset.state = state;
    el.stateName.textContent = t('state.' + state + '.name');
    el.stateNote.textContent = t('state.' + state + '.note');
    updateSosButton();
  }

  function tickDetector() {
    if (!detector.running) return;
    evaluateCriteria();

    const t0 = now();
    if (detector.state === 'idle' || detector.state === 'suspect') {
      if (analysis.motionPass && t0 >= detector.cooldownUntil) {
        if (!detector.patternSince) detector.patternSince = t0;
        detector.patternLost = 0;
        setState('suspect');
        if ((t0 - detector.patternSince) / 1000 >= settings.hold) startConfirming();
      } else if (detector.patternSince) {
        if (!detector.patternLost) detector.patternLost = t0;
        if (t0 - detector.patternLost > 1500) {
          detector.patternSince = 0;
          detector.patternLost = 0;
          setState('idle');
        }
      }
      if (detector.state === 'idle') learnBaseline();
    } else if (detector.state === 'confirming') {
      const left = Math.ceil((detector.confirmUntil - t0) / 1000);
      el.alarmCount.textContent = Math.max(0, left);
      renderAlarmTriggers();
      if (!analysis.motionPass) {
        if (!detector.patternLost) detector.patternLost = t0;
        if (t0 - detector.patternLost > 3000) return resolveRejected();
      } else {
        detector.patternLost = 0;
      }
      if (left <= 0) {
        if (bioConfirmed()) raiseAlarm('tonic');
        else resolveRejected();
      }
    }
    renderLive();
  }

  /** Slow drift of the resting baseline while nothing is happening. */
  let baselineSavedAt = 0;
  function learnBaseline() {
    if (analysis.amp > 0.6) return;
    if (vitals.hr > 30 && vitals.hr < 130) {
      settings.baselineHr = Math.round((settings.baselineHr * 0.995 + vitals.hr * 0.005) * 10) / 10;
    }
    if (vitals.spo2 > 85) {
      settings.baselineSpo2 = Math.round((settings.baselineSpo2 * 0.995 + vitals.spo2 * 0.005) * 10) / 10;
    }
    if (now() - baselineSavedAt > 60000) {
      baselineSavedAt = now();
      saveSettings();
    }
  }

  function startConfirming() {
    detector.confirmUntil = now() + settings.window * 1000;
    detector.patternLost = 0;
    setState('confirming');
    detector.current = {
      id: 'e' + now().toString(36) + Math.random().toString(36).slice(2, 6),
      startedAt: new Date().toISOString(),
      type: 'tonic',
      peakHr: vitals.hr,
      minSpo2: vitals.spo2,
      freq: analysis.freq,
      amp: analysis.amp,
      motionSource: sources.motion,
      vitalsSource: sources.vitals
    };
    openAlarmOverlay('confirming');
    if (settings.vibrate && navigator.vibrate) navigator.vibrate([120, 80, 120]);
  }

  function raiseAlarm(type) {
    setState('alarm');
    const ev = detector.current || {
      id: 'e' + now().toString(36) + Math.random().toString(36).slice(2, 6),
      startedAt: new Date().toISOString(),
      peakHr: vitals.hr,
      minSpo2: vitals.spo2,
      freq: analysis.freq,
      amp: analysis.amp,
      motionSource: sources.motion,
      vitalsSource: sources.vitals
    };
    ev.type = type;
    ev.outcome = 'notified';
    ev.notified = contacts.filter((c) => c.notify).map((c) => c.name);
    detector.current = ev;
    finalizeEvent(ev, POST_ROLL);
    openAlarmOverlay('sent');
    fireLocalAlarm();
  }

  function resolveCancelled() {
    const ev = detector.current;
    if (ev) {
      ev.type = ev.type === 'manual' ? 'manual' : 'tonic';
      ev.outcome = 'cancelled';
      ev.notified = [];
      finalizeEvent(ev, POST_ROLL_SHORT);
    }
    stopLocalAlarm();
    openAlarmOverlay('cancelled');
    backToIdle();
  }

  function resolveRejected() {
    const ev = detector.current;
    if (ev) {
      ev.type = 'rejected';
      ev.outcome = 'rejected';
      ev.notified = [];
      finalizeEvent(ev, POST_ROLL_SHORT);
    }
    openAlarmOverlay('rejected');
    toast(t('toast.rejected'), 'warn');
    backToIdle();
  }

  function backToIdle() {
    detector.patternSince = 0;
    detector.patternLost = 0;
    detector.confirmUntil = 0;
    detector.cooldownUntil = now() + COOLDOWN * 1000;
    detector.current = null;
    if (detector.running) setState('idle');
    else setState('off');
  }

  /* ====================================================================== */
  /* Recorder                                                               */
  /* ====================================================================== */
  function recordSample() {
    recorder.push({
      t: now(),
      acc: Math.round(analysis.amp * 100) / 100,
      hr: Math.round(vitals.hr),
      spo2: Math.round(vitals.spo2 * 10) / 10
    });
    const cutoff = now() - RECORD_SPAN * 1000;
    while (recorder.length && recorder[0].t < cutoff) recorder.shift();
  }

  function sliceWindow(startMs, postSeconds) {
    const from = startMs - PRE_ROLL * 1000;
    const to = startMs + postSeconds * 1000;
    return recorder
      .filter((s) => s.t >= from && s.t <= to)
      .map((s) => ({ o: Math.round((s.t - startMs) / 100) / 10, a: s.acc, h: s.hr, s: s.spo2 }));
  }

  /** Store the event immediately, then patch in the post-roll once recorded. */
  function finalizeEvent(ev, postSeconds) {
    const startMs = Date.parse(ev.startedAt);
    ev.window = sliceWindow(startMs, 0);
    ev.durationS = 0;
    events.unshift(ev);
    events = events.slice(0, 60);
    saveEvents();
    renderHistory();

    const collectUntil = startMs + postSeconds * 1000;
    const finish = () => {
      const stored = events.find((e) => e.id === ev.id);
      if (!stored) return;
      stored.window = sliceWindow(startMs, postSeconds);
      // How long the shaking actually lasted, not how long we kept recording.
      let lastShake = 0;
      for (const s of stored.window) {
        if (s.o >= 0 && s.a >= settings.ampMin) lastShake = s.o;
      }
      stored.durationS = Math.round(lastShake);
      stored.peakHr = Math.max(...stored.window.map((s) => s.h), stored.peakHr || 0);
      stored.minSpo2 = Math.min(...stored.window.map((s) => s.s), stored.minSpo2 || 100);
      stored.endedAt = new Date(collectUntil).toISOString();
      saveEvents();
      renderHistory();
    };
    setTimeout(finish, Math.max(500, collectUntil - now()));
  }

  /* ====================================================================== */
  /* Local alarm: sound, vibration, notification                            */
  /* ====================================================================== */
  let audioCtx = null;
  let beepTimer = null;

  function beep() {
    if (!settings.sound) return;
    try {
      if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
      if (audioCtx.state === 'suspended') audioCtx.resume();
      const osc = audioCtx.createOscillator();
      const gain = audioCtx.createGain();
      osc.type = 'square';
      osc.frequency.value = 880;
      gain.gain.setValueAtTime(0.0001, audioCtx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.18, audioCtx.currentTime + 0.02);
      gain.gain.exponentialRampToValueAtTime(0.0001, audioCtx.currentTime + 0.32);
      osc.connect(gain).connect(audioCtx.destination);
      osc.start();
      osc.stop(audioCtx.currentTime + 0.34);
    } catch (err) { /* audio unavailable */ }
  }

  function fireLocalAlarm() {
    stopLocalAlarm();
    beep();
    beepTimer = setInterval(beep, 1400);
    if (settings.vibrate && navigator.vibrate) navigator.vibrate([500, 200, 500, 200, 500]);
    if (settings.notifications && 'Notification' in window && Notification.permission === 'granted') {
      try {
        new Notification(t('notif.title'), {
          body: t('notif.body', { time: new Date().toLocaleTimeString(lang) }),
          tag: 'epi-alarm'
        });
      } catch (err) { /* notifications unavailable */ }
    }
  }

  function stopLocalAlarm() {
    if (beepTimer) { clearInterval(beepTimer); beepTimer = null; }
    if (navigator.vibrate) navigator.vibrate(0);
  }

  /* ====================================================================== */
  /* Alarm overlay                                                          */
  /* ====================================================================== */
  let geoUrl = '';

  function alarmMessage() {
    let body = t('sms.body', {
      time: new Date().toLocaleString(lang),
      hr: Math.round(vitals.hr),
      spo2: Math.round(vitals.spo2)
    });
    if (geoUrl) body += t('sms.location', { url: geoUrl });
    return body;
  }

  function renderAlarmTriggers() {
    const rows = [
      { label: t('alarm.trigger.freq'), value: analysis.freq.toFixed(1) + ' Hz', pass: analysis.motionPass },
      { label: t('alarm.trigger.amp'), value: analysis.amp.toFixed(1) + ' m/s²', pass: analysis.motionPass },
      { label: t('alarm.trigger.hr'), value: Math.round(vitals.hr) + ' bpm' + (analysis.hrPass ? ' ↑' : ''), pass: analysis.hrPass },
      { label: t('alarm.trigger.spo2'), value: Math.round(vitals.spo2) + '%' + (analysis.spo2Pass ? ' ↓' : ''), pass: analysis.spo2Pass }
    ];
    el.alarmTriggers.innerHTML = rows
      .map((r) => `<li><span>${escapeHtml(r.label)}</span><b>${escapeHtml(r.value)}</b></li>`)
      .join('');
  }

  function renderAlarmContacts() {
    const list = contacts.filter((c) => c.notify);
    if (!list.length) {
      el.alarmContacts.hidden = false;
      el.alarmContacts.innerHTML = `<p class="alarm-note">${escapeHtml(t('alarm.noContacts'))}</p>`;
      return;
    }
    const body = encodeURIComponent(alarmMessage());
    el.alarmContacts.hidden = false;
    el.alarmContacts.innerHTML = list.map((c) => `
      <div class="alarm-contact">
        <span class="who">${escapeHtml(c.name)}<small>${escapeHtml(c.relation || '')} ${escapeHtml(c.phone)}</small></span>
        <span class="contact-actions">
          <a class="mini" href="sms:${telNumber(c.phone)}?body=${body}">${escapeHtml(t('alarm.notifyBtn'))}</a>
          <a class="mini" href="tel:${telNumber(c.phone)}">${escapeHtml(t('alarm.callBtn'))}</a>
        </span>
      </div>`).join('');
  }

  function openAlarmOverlay(mode) {
    el.alarmOverlay.hidden = false;
    el.alarmOverlay.classList.toggle('is-resolved', mode !== 'confirming');
    el.alarmContacts.hidden = true;
    el.btnAlarmCancel.hidden = mode !== 'confirming';
    el.btnAlarmClose.hidden = mode === 'confirming';
    el.alarmCountWrap.hidden = mode !== 'confirming';

    if (mode === 'confirming') {
      el.alarmState.textContent = t('alarm.confirming');
      el.alarmTitle.textContent = t('alarm.confirmingTitle');
      el.alarmNote.textContent = t('alarm.confirmingNote');
      el.alarmCount.textContent = settings.window;
      renderAlarmTriggers();
      if (settings.geo) requestGeo();
    } else if (mode === 'sent') {
      el.alarmState.textContent = t('alarm.sent');
      el.alarmTitle.textContent = t('alarm.sentTitle');
      el.alarmNote.textContent = t('alarm.sentNote');
      renderAlarmTriggers();
      renderAlarmContacts();
    } else if (mode === 'cancelled') {
      el.alarmState.textContent = t('alarm.cancelled');
      el.alarmTitle.textContent = t('alarm.cancelledTitle');
      el.alarmNote.textContent = t('alarm.cancelledNote');
      el.alarmTriggers.innerHTML = '';
    } else if (mode === 'rejected') {
      el.alarmState.textContent = t('alarm.rejected');
      el.alarmTitle.textContent = t('alarm.rejectedTitle');
      el.alarmNote.textContent = t('alarm.rejectedNote');
      renderAlarmTriggers();
    }
  }

  function closeAlarmOverlay() {
    el.alarmOverlay.hidden = true;
    stopLocalAlarm();
    if (detector.state === 'alarm') backToIdle();
  }

  function requestGeo() {
    if (!navigator.geolocation) return;
    navigator.geolocation.getCurrentPosition(
      (pos) => {
        const { latitude, longitude } = pos.coords;
        geoUrl = `https://www.openstreetmap.org/?mlat=${latitude.toFixed(5)}&mlon=${longitude.toFixed(5)}#map=17/${latitude.toFixed(5)}/${longitude.toFixed(5)}`;
        if (!el.alarmContacts.hidden) renderAlarmContacts();
      },
      () => { geoUrl = ''; },
      { enableHighAccuracy: true, timeout: 8000, maximumAge: 60000 }
    );
  }

  /* ====================================================================== */
  /* Charts                                                                 */
  /* ====================================================================== */
  function fitCanvas(canvas) {
    const dpr = window.devicePixelRatio || 1;
    const width = canvas.clientWidth || 320;
    const height = parseInt(canvas.getAttribute('height'), 10) || 120;
    if (canvas.width !== Math.round(width * dpr) || canvas.height !== Math.round(height * dpr)) {
      canvas.width = Math.round(width * dpr);
      canvas.height = Math.round(height * dpr);
      canvas.style.height = height + 'px';
    }
    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, width, height);
    return { ctx, width, height };
  }

  function drawLine(ctx, values, width, height, min, max, color, lineWidth) {
    if (values.length < 2) return;
    const span = (max - min) || 1;
    ctx.beginPath();
    values.forEach((v, i) => {
      const x = (i / (values.length - 1)) * width;
      const y = height - ((Math.min(max, Math.max(min, v)) - min) / span) * height;
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    });
    ctx.strokeStyle = color;
    ctx.lineWidth = lineWidth || 1.6;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';
    ctx.stroke();
  }

  function drawPpg() {
    const { ctx, width, height } = fitCanvas(el.ppgChart);
    const alarmish = detector.state === 'confirming' || detector.state === 'alarm';
    drawLine(ctx, ppgTrace, width, height, -1.2, 1.6, alarmish ? '#ff4b46' : '#4fd18b', 1.6);
  }

  function drawMotion() {
    const { ctx, width, height } = fitCanvas(el.motionChart);
    const from = now() - 12000;
    const win = motionSamples.filter((s) => s.t >= from);
    if (win.length < 2) return;
    let sum = 0;
    for (const s of win) sum += s.mag;
    const mean = sum / win.length;
    const values = win.map((s) => s.mag - mean);
    const peak = Math.max(settings.ampMin * 2.2, ...values.map(Math.abs));

    const thrY = height / 2 - (settings.ampMin / peak) * (height / 2);
    ctx.beginPath();
    ctx.moveTo(0, thrY);
    ctx.lineTo(width, thrY);
    ctx.strokeStyle = 'rgba(211,34,30,0.55)';
    ctx.lineWidth = 1;
    ctx.setLineDash([4, 4]);
    ctx.stroke();
    ctx.setLineDash([]);

    drawLine(ctx, values, width, height, -peak, peak, analysis.motionPass ? '#ff4b46' : '#7aa2ff', 1.4);
  }

  function drawEventWindow(canvas, win) {
    const { ctx, width, height } = fitCanvas(canvas);
    if (!win || win.length < 2) return;
    const zeroIdx = win.findIndex((s) => s.o >= 0);
    if (zeroIdx > 0) {
      const x = (zeroIdx / (win.length - 1)) * width;
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, height);
      ctx.strokeStyle = 'rgba(244,243,238,0.25)';
      ctx.lineWidth = 1;
      ctx.setLineDash([3, 3]);
      ctx.stroke();
      ctx.setLineDash([]);
    }
    drawLine(ctx, win.map((s) => s.a), width, height, 0, Math.max(6, ...win.map((s) => s.a)), '#7aa2ff', 1.5);
    drawLine(ctx, win.map((s) => s.h), width, height, 40, 180, '#ff4b46', 1.5);
    drawLine(ctx, win.map((s) => s.s), width, height, 80, 100, '#4fd18b', 1.5);
  }

  /* ====================================================================== */
  /* Live rendering                                                         */
  /* ====================================================================== */
  function updateSourceChips() {
    const motionLabel = sources.motion === 'device' ? t('source.motion') : t('source.sim');
    const vitalsLabel = sources.vitals === 'ble' ? t('source.ble') : t('source.sim');
    el.motionChip.textContent = motionLabel;
    el.ppgChip.textContent = vitalsLabel + (detector.state === 'idle' || detector.state === 'off' ? ' · 25 Hz' : ' · 100 Hz');
    const live = sources.motion === 'device' || sources.vitals === 'ble';
    el.linkPill.dataset.state = live ? 'live' : 'sim';
    el.linkLabel.textContent = live ? `${motionLabel} + ${vitalsLabel}` : t('header.offline');
    el.btnConnect.textContent = sources.vitals === 'ble' ? t('monitor.connected') : t('monitor.connect');
  }

  function renderLive() {
    el.mHr.textContent = detector.running ? Math.round(vitals.hr) : '—';
    el.mSpo2.textContent = detector.running ? Math.round(vitals.spo2) : '—';
    el.mAmp.textContent = detector.running ? analysis.amp.toFixed(1) : '—';
    el.mFreq.textContent = detector.running ? analysis.freq.toFixed(1) : '—';
    el.mHrBase.textContent = `baza ${Math.round(settings.baselineHr)} bpm`;
    el.mSpo2Base.textContent = `baza ${Math.round(settings.baselineSpo2)}%`;
    el.mAmpThr.textContent = `próg ${settings.ampMin.toFixed(1)}`;
    el.mFreqThr.textContent = `${settings.freqMin.toFixed(1)}–${settings.freqMax.toFixed(1)}`;
    el.mHr.parentElement.parentElement.classList.toggle('hot', analysis.hrPass);
    el.mSpo2.parentElement.parentElement.classList.toggle('hot', analysis.spo2Pass);
    el.mAmp.parentElement.parentElement.classList.toggle('warn', analysis.motionPass);
    el.mFreq.parentElement.parentElement.classList.toggle('warn', analysis.motionPass);

    el.critMotion.textContent = analysis.freq.toFixed(1) + ' Hz / ' + analysis.amp.toFixed(1) + ' m/s²';
    el.critHr.textContent = Math.round(vitals.hr) + ' bpm · ' + (analysis.hrSlope >= 0 ? '+' : '') + analysis.hrSlope.toFixed(1) + ' bpm/s';
    el.critSpo2.textContent = Math.round(vitals.spo2) + ' %';
    el.criteria.querySelector('[data-key="motion"]').dataset.pass = String(analysis.motionPass);
    el.criteria.querySelector('[data-key="hr"]').dataset.pass = String(analysis.hrPass);
    el.criteria.querySelector('[data-key="spo2"]').dataset.pass = String(analysis.spo2Pass);
  }

  function updateSosButton() {
    const cancelMode = detector.state === 'confirming';
    el.sosLabel.textContent = cancelMode ? t('sos.cancel') : 'SOS';
    el.sosNote.textContent = cancelMode ? t('sos.cancelNote') : t('sos.note');
    el.btnSos.classList.toggle('armed', cancelMode);
  }

  /* ====================================================================== */
  /* History                                                                */
  /* ====================================================================== */
  const kindLabel = (ev) => {
    if (ev.type === 'manual') return t('event.manual');
    if (ev.type === 'rejected') return t('event.rejected');
    return t('event.tonic');
  };
  const outcomeTag = (ev) => {
    if (ev.outcome === 'cancelled') return { cls: 'tag-cancelled', text: t('event.cancelled') };
    if (ev.outcome === 'rejected') return { cls: 'tag-rejected', text: t('event.rejectedTag') };
    return { cls: 'tag-notified', text: t('event.notified') };
  };

  /** Keep only what a dialer accepts, so hrefs stay clean and unambiguous. */
  const telNumber = (phone) => String(phone || '').replace(/[^\d+]/g, '');

  function escapeHtml(value) {
    return String(value == null ? '' : value)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
  }

  function formatWhen(iso) {
    const d = new Date(iso);
    return d.toLocaleString(lang, { day: '2-digit', month: '2-digit', year: 'numeric', hour: '2-digit', minute: '2-digit' });
  }

  function renderHistory() {
    el.historyCount.textContent = String(events.length);
    el.historyEmpty.hidden = events.length > 0;
    el.eventList.innerHTML = events.map((ev) => {
      const tag = outcomeTag(ev);
      return `<li><button type="button" class="event" data-id="${escapeHtml(ev.id)}">
        <span class="event-top">
          <span class="event-when">${escapeHtml(formatWhen(ev.startedAt))}</span>
          <span class="tag ${tag.cls}">${escapeHtml(tag.text)}</span>
        </span>
        <span class="event-kind">${escapeHtml(kindLabel(ev))}</span>
        <span class="event-meta">${Math.round(ev.peakHr || 0)} bpm · ${Math.round(ev.minSpo2 || 0)}% · ${(ev.freq || 0).toFixed(1)} Hz</span>
      </button></li>`;
    }).join('');
  }

  function openEventSheet(id) {
    const ev = events.find((e) => e.id === id);
    if (!ev) return;
    el.eventSheet.hidden = false;
    el.eventSheet.dataset.id = id;
    el.sheetWhen.textContent = formatWhen(ev.startedAt);
    el.sheetTitle.textContent = kindLabel(ev);
    const facts = [
      [t('sheet.duration'), (ev.durationS || 0) + ' s'],
      [t('sheet.peakHr'), Math.round(ev.peakHr || 0) + ' bpm'],
      [t('sheet.minSpo2'), Math.round(ev.minSpo2 || 0) + ' %'],
      [t('sheet.freq'), (ev.freq || 0).toFixed(1) + ' Hz'],
      [t('sheet.amp'), (ev.amp || 0).toFixed(1) + ' m/s²'],
      [t('sheet.source'), (ev.motionSource === 'device' ? t('source.motion') : t('source.sim')) + ' + ' + (ev.vitalsSource === 'ble' ? t('source.ble') : t('source.sim'))],
      [t('sheet.notified'), (ev.notified && ev.notified.length) ? ev.notified.join(', ') : t('sheet.none')]
    ];
    el.sheetFacts.innerHTML = facts
      .map(([k, v]) => `<div><dt>${escapeHtml(k)}</dt><dd>${escapeHtml(v)}</dd></div>`)
      .join('');
    requestAnimationFrame(() => drawEventWindow(el.sheetChart, ev.window));
  }

  /* ====================================================================== */
  /* Contacts                                                               */
  /* ====================================================================== */
  let editingContact = null;

  function renderContacts() {
    el.contactsCount.textContent = String(contacts.length);
    el.contactsEmpty.hidden = contacts.length > 0;
    el.contactList.innerHTML = contacts.map((c) => `
      <li class="contact">
        <span class="contact-top">
          <span>
            <span class="contact-name">${escapeHtml(c.name)}</span>
            <span class="contact-meta">${escapeHtml(c.relation || '')} ${escapeHtml(c.phone)}</span>
          </span>
          ${c.notify ? '' : `<span class="muted-flag">${escapeHtml(t('contacts.silent'))}</span>`}
        </span>
        <span class="contact-actions">
          <a class="mini" href="sms:${telNumber(c.phone)}">${escapeHtml(t('contacts.sms'))}</a>
          <a class="mini" href="tel:${telNumber(c.phone)}">${escapeHtml(t('contacts.call'))}</a>
          <button type="button" class="mini" data-edit="${escapeHtml(c.id)}">${escapeHtml(t('contacts.editBtn'))}</button>
          <button type="button" class="mini danger" data-remove="${escapeHtml(c.id)}">${escapeHtml(t('contacts.delete'))}</button>
        </span>
      </li>`).join('');
  }

  function startEditContact(id) {
    const c = contacts.find((x) => x.id === id);
    if (!c) return;
    editingContact = id;
    el.cName.value = c.name;
    el.cRelation.value = c.relation || '';
    el.cPhone.value = c.phone;
    el.cNotify.checked = !!c.notify;
    el.contactCancel.hidden = false;
    el.contactForm.querySelector('h2').textContent = t('contacts.edit');
    el.contactForm.scrollIntoView({ behavior: 'smooth', block: 'center' });
  }

  function resetContactForm() {
    editingContact = null;
    el.contactForm.reset();
    el.cNotify.checked = true;
    el.contactCancel.hidden = true;
    el.contactForm.querySelector('h2').textContent = t('contacts.add');
  }

  /* ====================================================================== */
  /* Export                                                                 */
  /* ====================================================================== */
  function download(filename, text, mime) {
    // Spreadsheets need the BOM to read UTF-8 CSV; JSON parsers choke on it.
    const payload = mime === 'text/csv' ? '﻿' + text : text;
    const blob = new Blob([payload], { type: mime + ';charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
    toast(t('toast.exported'));
  }

  const csvCell = (value) => {
    const s = String(value == null ? '' : value);
    return /[",;\n]/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s;
  };
  const csvRows = (rows) => rows.map((r) => r.map(csvCell).join(',')).join('\n');

  function exportEventsCsv() {
    if (!events.length) return toast(t('toast.noEvents'), 'warn');
    const rows = [['id', 'started_at', 'ended_at', 'type', 'outcome', 'shaking_s', 'peak_hr_bpm', 'min_spo2_pct', 'freq_hz', 'amp_ms2', 'motion_source', 'vitals_source', 'notified']];
    for (const ev of events) {
      rows.push([ev.id, ev.startedAt, ev.endedAt || '', ev.type, ev.outcome, ev.durationS || 0,
        Math.round(ev.peakHr || 0), ev.minSpo2 || '', (ev.freq || 0).toFixed(2), (ev.amp || 0).toFixed(2),
        ev.motionSource || '', ev.vitalsSource || '', (ev.notified || []).join('; ')]);
    }
    download(lang === 'en' ? 'epi-events.csv' : 'epi-zdarzenia.csv', csvRows(rows), 'text/csv');
  }

  function exportWindowCsv(ev) {
    const rows = [['offset_s', 'motion_rms_ms2', 'hr_bpm', 'spo2_pct']];
    for (const s of (ev.window || [])) rows.push([s.o, s.a, s.h, s.s]);
    download('epi-' + ev.id + '.csv', csvRows(rows), 'text/csv');
  }

  function exportJson() {
    download(lang === 'en' ? 'epi-data.json' : 'epi-dane.json', JSON.stringify({ version: VERSION, exportedAt: new Date().toISOString(), settings, contacts, events }, null, 2), 'application/json');
  }

  function printReport() {
    if (!events.length) return toast(t('toast.noEvents'), 'warn');
    const head = [t('print.when'), t('print.kind'), t('print.outcome'), t('print.duration'), t('print.hr'), t('print.spo2')];
    const body = events.map((ev) => `<tr>
      <td>${escapeHtml(formatWhen(ev.startedAt))}</td>
      <td>${escapeHtml(kindLabel(ev))}</td>
      <td>${escapeHtml(outcomeTag(ev).text)}</td>
      <td>${ev.durationS || 0} s</td>
      <td>${Math.round(ev.peakHr || 0)} bpm</td>
      <td>${Math.round(ev.minSpo2 || 0)} %</td></tr>`).join('');
    el.printReport.innerHTML = `
      <h1>${escapeHtml(t('print.title'))}</h1>
      <p class="sub">${escapeHtml(t('print.sub', { date: new Date().toLocaleString(lang) }))}</p>
      <table><thead><tr>${head.map((h) => `<th>${escapeHtml(h)}</th>`).join('')}</tr></thead><tbody>${body}</tbody></table>
      <p class="foot">${escapeHtml(t('print.foot'))}</p>`;
    window.print();
  }

  /* ====================================================================== */
  /* Settings binding                                                       */
  /* ====================================================================== */
  function renderSettings() {
    el.setAmp.value = settings.ampMin;
    el.setFreqMin.value = settings.freqMin;
    el.setFreqMax.value = settings.freqMax;
    el.setHold.value = settings.hold;
    el.setWindow.value = settings.window;
    el.setHr.value = settings.hrRise;
    el.setSpo2.value = settings.spo2Drop;
    el.setBoth.checked = settings.requireBoth;
    el.setBaseHr.value = Math.round(settings.baselineHr);
    el.setBaseSpo2.value = Math.round(settings.baselineSpo2);
    el.setMotion.checked = settings.useMotion;
    el.setSound.checked = settings.sound;
    el.setVibrate.checked = settings.vibrate;
    el.setNotify.checked = settings.notifications;
    el.setGeo.checked = settings.geo;
    renderSettingValues();
    el.storageInfo.textContent = t('settings.storage', {
      events: events.length, contacts: contacts.length, size: store.size()
    });
    el.fusionNote.textContent = settings.requireBoth ? t('fusion.noteBoth') : t('fusion.note');
  }

  function renderSettingValues() {
    $('valAmp').textContent = Number(settings.ampMin).toFixed(1);
    $('valFreqMin').textContent = Number(settings.freqMin).toFixed(1);
    $('valFreqMax').textContent = Number(settings.freqMax).toFixed(1);
    $('valHold').textContent = settings.hold;
    $('valWindow').textContent = settings.window;
    $('valHr').textContent = settings.hrRise;
    $('valSpo2').textContent = settings.spo2Drop;
  }

  function bindRange(input, key, parse) {
    input.addEventListener('input', () => {
      settings[key] = parse(input.value);
      if (settings.freqMin >= settings.freqMax) {
        settings.freqMax = Math.min(10, settings.freqMin + 0.5);
        el.setFreqMax.value = settings.freqMax;
      }
      renderSettingValues();
      renderLive();
      saveSettings();
    });
  }

  /* ====================================================================== */
  /* Views                                                                  */
  /* ====================================================================== */
  const VIEWS = ['monitor', 'history', 'contacts', 'settings'];

  function showView(name) {
    const view = VIEWS.includes(name) ? name : 'monitor';
    VIEWS.forEach((v) => { $('view-' + v).hidden = v !== view; });
    el.tabbar.querySelectorAll('.tab').forEach((tab) => {
      if (tab.dataset.view === view) tab.setAttribute('aria-current', 'page');
      else tab.removeAttribute('aria-current');
    });
    if (location.hash.slice(1) !== view) history.replaceState(null, '', '#' + view);
    if (view === 'settings') renderSettings();
    el.appMain.scrollTop = 0;
    window.scrollTo(0, 0);
  }

  /* ====================================================================== */
  /* Language                                                               */
  /* ====================================================================== */
  function applyLanguage(next) {
    lang = STRINGS[next] ? next : 'pl';
    document.documentElement.lang = lang;
    document.title = t('doc.title');
    document.querySelectorAll('[data-i18n]').forEach((node) => {
      node.textContent = t(node.dataset.i18n);
    });
    el.langPl.setAttribute('aria-current', String(lang === 'pl'));
    el.langEn.setAttribute('aria-current', String(lang === 'en'));
    el.exitLink.href = lang === 'en' ? '../en/projects.html#epi-app' : '../projekty.html#epi-aplikacja';
    el.btnRun.textContent = detector.running ? t('monitor.stop') : t('monitor.start');
    el.stateName.textContent = t('state.' + detector.state + '.name');
    el.stateNote.textContent = t('state.' + detector.state + '.note');
    el.bleSupport.textContent = navigator.bluetooth ? t('ble.supported') : t('ble.unsupported');
    reportMotionStatus();
    el.fusionNote.textContent = settings.requireBoth ? t('fusion.noteBoth') : t('fusion.note');
    updateSourceChips();
    updateSosButton();
    renderLive();
    renderHistory();
    renderContacts();
    renderSettings();
  }

  /* ====================================================================== */
  /* Run loop                                                               */
  /* ====================================================================== */
  const SIM_HZ = 25;
  let simTimer = null;
  let detectTimer = null;
  let recordTimer = null;
  let hrTimer = null;
  let rafId = null;
  let wakeLock = null;
  let ppgPhase = 0;
  let lastFrame = 0;

  function ppgTick(dt) {
    const rate = Math.max(30, vitals.hr) / 60;
    ppgPhase += rate * dt;
    if (ppgPhase >= 1) ppgPhase -= 1;
    // Synthetic pulse wave: systolic peak plus a smaller dicrotic notch.
    const p = ppgPhase;
    const value = Math.exp(-Math.pow((p - 0.12) / 0.06, 2)) * 1.4
      + Math.exp(-Math.pow((p - 0.34) / 0.09, 2)) * 0.45
      - 0.25 + (Math.random() - 0.5) * 0.05;
    ppgTrace.push(value);
    ppgTrace.shift();
  }

  function frame(ts) {
    rafId = requestAnimationFrame(frame);
    if (document.hidden) return;
    const dt = lastFrame ? Math.min(0.1, (ts - lastFrame) / 1000) : 0.02;
    lastFrame = ts;
    if (detector.running) ppgTick(dt);
    drawPpg();
    drawMotion();
  }

  async function requestWakeLock() {
    try {
      if ('wakeLock' in navigator) wakeLock = await navigator.wakeLock.request('screen');
    } catch (err) { /* not critical */ }
  }

  function releaseWakeLock() {
    if (wakeLock) { wakeLock.release().catch(() => {}); wakeLock = null; }
  }

  async function startMonitoring() {
    if (detector.running) return;
    detector.running = true;
    detector.cooldownUntil = 0;
    setState('idle');
    el.btnRun.textContent = t('monitor.stop');
    setSimMode('rest');
    sim.hr = settings.baselineHr;
    sim.spo2 = settings.baselineSpo2;

    if (settings.useMotion && sources.motion !== 'device') await enableMotion();

    let last = now();
    simTimer = setInterval(() => {
      const t1 = now();
      const dt = Math.min(0.2, (t1 - last) / 1000);
      last = t1;
      simStep(dt);
    }, 1000 / SIM_HZ);
    detectTimer = setInterval(tickDetector, 250);
    recordTimer = setInterval(recordSample, 1000 / RECORD_HZ);
    hrTimer = setInterval(() => {
      hrHistory.push({ t: now(), hr: vitals.hr });
      while (hrHistory.length && hrHistory[0].t < now() - 60000) hrHistory.shift();
    }, 1000);

    requestWakeLock();
    updateSourceChips();
    toast(t('toast.started'));
  }

  function stopMonitoring() {
    detector.running = false;
    [simTimer, detectTimer, recordTimer, hrTimer].forEach((id) => id && clearInterval(id));
    simTimer = detectTimer = recordTimer = hrTimer = null;
    setSimMode('rest');
    stopLocalAlarm();
    releaseWakeLock();
    backToIdle();
    setState('off');
    el.btnRun.textContent = t('monitor.start');
    renderLive();
    toast(t('toast.stopped'));
  }

  /* ====================================================================== */
  /* Events wiring                                                          */
  /* ====================================================================== */
  el.btnRun.addEventListener('click', () => {
    if (detector.running) stopMonitoring(); else startMonitoring();
  });
  el.btnConnect.addEventListener('click', connectBle);
  el.btnBle.addEventListener('click', connectBle);

  el.btnMotionPerm.addEventListener('click', async () => {
    const ok = await enableMotion();
    settings.useMotion = ok;
    el.setMotion.checked = ok;
    saveSettings();
    reportMotionStatus();
  });

  el.btnSimSeizure.addEventListener('click', () => {
    if (!detector.running) startMonitoring();
    detector.cooldownUntil = 0;
    setSimMode('seizure');
    toast(t('toast.simSeizure'));
  });
  el.btnSimActivity.addEventListener('click', () => {
    if (!detector.running) startMonitoring();
    detector.cooldownUntil = 0;
    setSimMode('activity');
    toast(t('toast.simActivity'));
  });
  el.btnSimStop.addEventListener('click', () => {
    setSimMode('rest');
    toast(t('toast.simStop'));
  });

  // SOS: hold to raise, single press to cancel a running confirmation.
  let holdTimer = null;
  let holdStart = 0;
  function holdProgress() {
    const p = Math.min(1, (now() - holdStart) / 1000);
    el.btnSos.style.setProperty('--hold', p);
    if (p >= 1) {
      releaseSos(true);
      manualSos();
    }
  }
  function pressSos(event) {
    event.preventDefault();
    if (detector.state === 'confirming') { resolveCancelled(); return; }
    holdStart = now();
    el.btnSos.classList.add('armed');
    // Keep the press alive even if the layout shifts under the finger.
    if (el.btnSos.setPointerCapture) {
      try { el.btnSos.setPointerCapture(event.pointerId); } catch (err) { /* ignore */ }
    }
    holdTimer = setInterval(holdProgress, 50);
  }
  function releaseSos(fired) {
    if (holdTimer) { clearInterval(holdTimer); holdTimer = null; }
    el.btnSos.style.setProperty('--hold', 0);
    if (!fired) el.btnSos.classList.remove('armed');
  }
  function manualSos() {
    if (!detector.running) startMonitoring();
    detector.current = {
      id: 'e' + now().toString(36) + Math.random().toString(36).slice(2, 6),
      startedAt: new Date().toISOString(),
      type: 'manual',
      peakHr: vitals.hr,
      minSpo2: vitals.spo2,
      freq: analysis.freq,
      amp: analysis.amp,
      motionSource: sources.motion,
      vitalsSource: sources.vitals
    };
    if (settings.geo) requestGeo();
    raiseAlarm('manual');
    el.alarmState.textContent = t('alarm.manual');
    el.alarmTitle.textContent = t('alarm.manualTitle');
  }
  el.btnSos.addEventListener('pointerdown', pressSos);
  el.btnSos.addEventListener('pointerup', () => releaseSos(false));
  el.btnSos.addEventListener('pointercancel', () => releaseSos(false));

  el.btnAlarmCancel.addEventListener('click', resolveCancelled);
  el.btnAlarmClose.addEventListener('click', closeAlarmOverlay);

  el.tabbar.addEventListener('click', (event) => {
    const tab = event.target.closest('.tab');
    if (tab) showView(tab.dataset.view);
  });

  el.eventList.addEventListener('click', (event) => {
    const btn = event.target.closest('.event');
    if (btn) openEventSheet(btn.dataset.id);
  });
  el.sheetClose.addEventListener('click', () => { el.eventSheet.hidden = true; });
  el.eventSheet.addEventListener('click', (event) => {
    if (event.target === el.eventSheet) el.eventSheet.hidden = true;
  });
  el.btnSheetCsv.addEventListener('click', () => {
    const ev = events.find((e) => e.id === el.eventSheet.dataset.id);
    if (ev) exportWindowCsv(ev);
  });
  el.btnSheetDelete.addEventListener('click', () => {
    events = events.filter((e) => e.id !== el.eventSheet.dataset.id);
    saveEvents();
    renderHistory();
    el.eventSheet.hidden = true;
    toast(t('event.deleted'));
  });

  el.btnExportCsv.addEventListener('click', exportEventsCsv);
  el.btnExportPdf.addEventListener('click', printReport);
  el.btnClearHistory.addEventListener('click', () => {
    if (!events.length) return toast(t('toast.noEvents'), 'warn');
    if (!confirm(t('history.confirmClear'))) return;
    events = [];
    saveEvents();
    renderHistory();
  });

  el.contactForm.addEventListener('submit', (event) => {
    event.preventDefault();
    const name = el.cName.value.trim();
    const phone = el.cPhone.value.trim();
    if (!name || !phone) return;
    const payload = { name, relation: el.cRelation.value.trim(), phone, notify: el.cNotify.checked };
    if (editingContact) {
      const c = contacts.find((x) => x.id === editingContact);
      if (c) Object.assign(c, payload);
    } else {
      contacts.push(Object.assign({ id: 'c' + now().toString(36) }, payload));
    }
    saveContacts();
    renderContacts();
    resetContactForm();
    toast(t('contacts.saved'));
  });
  el.contactCancel.addEventListener('click', resetContactForm);
  el.contactList.addEventListener('click', (event) => {
    const edit = event.target.closest('[data-edit]');
    if (edit) return startEditContact(edit.dataset.edit);
    const remove = event.target.closest('[data-remove]');
    if (remove) {
      contacts = contacts.filter((c) => c.id !== remove.dataset.remove);
      saveContacts();
      renderContacts();
      if (editingContact === remove.dataset.remove) resetContactForm();
      toast(t('contacts.removed'));
    }
  });

  bindRange(el.setAmp, 'ampMin', parseFloat);
  bindRange(el.setFreqMin, 'freqMin', parseFloat);
  bindRange(el.setFreqMax, 'freqMax', parseFloat);
  bindRange(el.setHold, 'hold', (v) => parseInt(v, 10));
  bindRange(el.setWindow, 'window', (v) => parseInt(v, 10));
  bindRange(el.setHr, 'hrRise', (v) => parseInt(v, 10));
  bindRange(el.setSpo2, 'spo2Drop', (v) => parseInt(v, 10));

  el.setBoth.addEventListener('change', () => {
    settings.requireBoth = el.setBoth.checked;
    el.fusionNote.textContent = settings.requireBoth ? t('fusion.noteBoth') : t('fusion.note');
    saveSettings();
  });
  el.btnResetThresholds.addEventListener('click', () => {
    THRESHOLD_KEYS.forEach((key) => { settings[key] = DEFAULTS[key]; });
    saveSettings();
    renderSettings();
    renderLive();
  });
  el.setBaseHr.addEventListener('change', () => {
    const v = parseInt(el.setBaseHr.value, 10);
    if (v >= 35 && v <= 120) { settings.baselineHr = v; saveSettings(); renderLive(); }
  });
  el.setBaseSpo2.addEventListener('change', () => {
    const v = parseInt(el.setBaseSpo2.value, 10);
    if (v >= 88 && v <= 100) { settings.baselineSpo2 = v; saveSettings(); renderLive(); }
  });

  el.setMotion.addEventListener('change', async () => {
    settings.useMotion = el.setMotion.checked;
    saveSettings();
    if (settings.useMotion) {
      const ok = await enableMotion();
      if (!ok) { settings.useMotion = false; el.setMotion.checked = false; saveSettings(); }
    } else {
      disableMotion();
    }
  });
  el.setSound.addEventListener('change', () => { settings.sound = el.setSound.checked; saveSettings(); });
  el.setVibrate.addEventListener('change', () => { settings.vibrate = el.setVibrate.checked; saveSettings(); });
  el.setGeo.addEventListener('change', () => { settings.geo = el.setGeo.checked; saveSettings(); });
  el.setNotify.addEventListener('change', async () => {
    if (!el.setNotify.checked) { settings.notifications = false; saveSettings(); return; }
    if (!('Notification' in window)) { el.setNotify.checked = false; return toast(t('toast.notifBlocked'), 'warn'); }
    const permission = await Notification.requestPermission();
    settings.notifications = permission === 'granted';
    el.setNotify.checked = settings.notifications;
    if (!settings.notifications) toast(t('toast.notifBlocked'), 'warn');
    saveSettings();
  });

  el.btnExportJson.addEventListener('click', exportJson);
  el.btnWipe.addEventListener('click', () => {
    if (!confirm(t('settings.confirmWipe'))) return;
    Object.values(KEYS).forEach(store.remove);
    events = [];
    contacts = [];
    settings = Object.assign({}, DEFAULTS, { lang });
    renderHistory();
    renderContacts();
    renderSettings();
    renderLive();
    toast(t('toast.wiped'));
  });

  [el.langPl, el.langEn].forEach((btn) => btn.addEventListener('click', () => {
    settings.lang = btn.dataset.lang;
    saveSettings();
    applyLanguage(settings.lang);
  }));

  window.addEventListener('hashchange', () => showView(location.hash.slice(1)));
  window.addEventListener('resize', () => { drawPpg(); drawMotion(); });
  document.addEventListener('visibilitychange', () => {
    if (!document.hidden && detector.running && !wakeLock) requestWakeLock();
  });

  /* ====================================================================== */
  /* Battery                                                                */
  /* ====================================================================== */
  function initBattery() {
    if (!navigator.getBattery) {
      el.batteryPill.textContent = '78 %';
      return;
    }
    navigator.getBattery().then((battery) => {
      const render = () => {
        el.batteryPill.textContent = Math.round(battery.level * 100) + ' %' + (battery.charging ? ' ⚡' : '');
      };
      render();
      battery.addEventListener('levelchange', render);
      battery.addEventListener('chargingchange', render);
    }).catch(() => { el.batteryPill.textContent = '78 %'; });
  }

  /* ====================================================================== */
  /* Boot                                                                   */
  /* ====================================================================== */
  function boot() {
    const params = new URLSearchParams(location.search);
    const requested = params.get('lang') || settings.lang
      || ((navigator.language || 'pl').toLowerCase().startsWith('pl') ? 'pl' : 'en');
    settings.lang = STRINGS[requested] ? requested : 'pl';
    saveSettings();

    el.appVersion.textContent = 'v' + VERSION;
    applyLanguage(settings.lang);
    showView(location.hash.slice(1) || 'monitor');
    initBattery();
    renderLive();
    rafId = requestAnimationFrame(frame);

    if ('serviceWorker' in navigator && location.protocol !== 'file:') {
      navigator.serviceWorker.register('sw.js').catch(() => { /* offline cache is optional */ });
    }
  }

  boot();
})();
