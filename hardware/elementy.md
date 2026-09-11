# Elementy bierne i półprzewodniki

Wykaz tego, co trzeba dołożyć poza modułami z listy części: rezystory,
tranzystory, diody i kondensatory, razem z wyliczeniem wartości. Osobno dla
wersji na Arduino Nano (logika 5 V) i na XIAO nRF54L15 (logika 3,3 V), bo
napięcie zasilania zmienia połowę wartości.

Rezystory: metalizowane 1 %, 0,25 W w wersji przewlekanej albo 0805 w wersji
do montażu powierzchniowego. Wszystkie moce strat wychodzą poniżej 20 mW poza
jednym miejscem, które jest wyraźnie oznaczone.

Kondensatory ceramiczne: X7R (albo X5R przy 10 µF i większych), na napięcie co
najmniej dwa razy wyższe niż w układzie, czyli 16 V albo 50 V. Kondensator
Y5V o tej samej pojemności traci przy napięciu roboczym większość pojemności
i nie nadaje się do odsprzęgania.

## Dioda RGB

Prąd docelowy 2–3 mA na kanał. Napięcie przewodzenia zależy od barwy: czerwona
struktura około 2,0 V, zielona i niebieska około 3,0 V. Rezystor liczony ze
wzoru R = (Uzas − Uf) / I.

| Wersja | Kanał | Rezystor | Prąd wyjściowy |
| --- | --- | --- | --- |
| Arduino Nano, 5 V | czerwony | 1 kΩ | 3,0 mA |
| Arduino Nano, 5 V | zielony i niebieski | 680 Ω | 2,9 mA |
| XIAO, 3,3 V | czerwony | 470 Ω | 2,8 mA |
| XIAO, 3,3 V | zielony i niebieski | 100 Ω | 3,0 mA przy Uf = 3,0 V |

Przy zasilaniu 3,3 V kanał zielony i niebieski pracuje na granicy: jeżeli
napięcie przewodzenia wyjdzie 3,4 V zamiast 3,0 V, dioda ledwie świeci, a przy
2,8 V prąd rośnie do 5 mA. Przy tym zasilaniu trzeba wybrać diodę o napięciu
przewodzenia nie wyższym niż 2,9 V dla zieleni i błękitu, a ostateczną jasność
dobrać współczynnikami `EPI_LED_GAIN_*` w firmwarze, nie rezystorem.

Trzy rezystory, po jednym na kanał, między katodą a wyprowadzeniem
mikrokontrolera. Przy diodzie ze wspólną anodą anoda idzie do zasilania.

## Silnik wibracyjny MT35

Pobiera około 90 mA przy 3 V, czyli więcej, niż wolno wyciągnąć z wyprowadzenia
mikrokontrolera. Potrzebny klucz, dioda gaszeniowa i odsprzęganie.

Wariant przewlekany, najprostszy do zmontowania:

| Element | Typ | Rola |
| --- | --- | --- |
| Tranzystor | BC337-40 (NPN, TO-92, Ic do 800 mA) | klucz po stronie masy |
| Rezystor bazy | 1 kΩ | prąd bazy 2,6 mA przy 3,3 V, 4,3 mA przy 5 V |
| Rezystor baza–emiter | 100 kΩ | trzyma tranzystor zamknięty, dopóki wyprowadzenie jest w stanie wysokiej impedancji |
| Dioda gaszeniowa | 1N4148 | katodą do plusa zasilania silnika, anodą do kolektora |
| Kondensator przy silniku | 100 nF X7R 50 V | tłumi iskrzenie szczotek |

Prąd bazy 2,6 mA przy wzmocnieniu co najmniej 250 daje zapas nasycenia rzędu
siedmiu razy ponad 90 mA, więc napięcie kolektor–emiter zostaje w granicach
0,2 V.

Wariant do montażu powierzchniowego: AO3400A albo IRLML2502 zamiast
tranzystora bipolarnego, rezystor bramki 100 Ω, rezystor bramka–źródło 100 kΩ,
ta sama dioda i ten sam kondensator. AO3400A ma napięcie progowe poniżej 1,5 V
i rezystancję kanału rzędu 30 mΩ przy 4,5 V na bramce, więc na kluczu nie
zostaje nic. 2N7002 w tym miejscu jest złym wyborem: jego rezystancja kanału to
około 7,5 Ω, czyli przy 90 mA spadek 0,7 V, a dopuszczalny prąd ciągły 210 mA
nie zostawia zapasu na rozruch.

Zasilanie silnika. MT35 jest na 3 V, więc najlepiej z szyny 3,3 V. Podanie 5 V
skraca jego życie; jeżeli nie ma innej szyny, w szereg idzie rezystor 22 Ω
0,5 W, który przy 90 mA zabiera 2 V i rozprasza 0,18 W. To jedyne miejsce
w całym układzie, gdzie rezystor 0,25 W nie wystarcza.

Rozruch silnika bierze dwa do trzech razy więcej prądu niż praca ustalona, więc
na szynie, z której jest zasilany, musi siedzieć kondensator 100 µF
elektrolitowy niskoimpedancyjny 10 V albo 16 V. Bez niego szarpnięcie napięcia
potrafi zresetować czujniki na tej samej szynie.

## Buzzer

Ten z listy części bierze do 30 mA przy 5 V, czyli powyżej zalecanego obciążenia
wyprowadzenia ATmegi i poza zasięgiem wyprowadzenia nRF54L15. Klucz taki sam
jak przy silniku: BC337-40 z rezystorem bazy 1 kΩ i rezystorem baza–emiter
100 kΩ. Przetwornik magnetoelektryczny ma cewkę, więc dioda 1N4148 w tej samej
konfiguracji co przy silniku jest potrzebna także tutaj. Do tego 100 nF X7R
przy zaciskach.

Przy przetworniku piezoelektrycznym bez generatora, sterowanym PWM, dioda
odpada, bo obciążenie jest pojemnościowe, ale klucz zostaje.

## Przycisk

Tact switch zwiera wyprowadzenie do masy, podciągnięcie jest wewnętrzne w
mikrokontrolerze, więc rezystor zewnętrzny nie jest potrzebny. Warto dołożyć
100 nF X7R między wyprowadzenie a masę: z podciągnięciem rzędu 30 kΩ daje to
stałą czasową około 3 ms, czyli dziesięć razy krócej niż filtrowanie programowe,
a zbiera zakłócenia z długiego przewodu do przycisku.

## Pomiar napięcia ogniwa

Dzielnik 1:2 z dwóch rezystorów 100 kΩ, między plusem ogniwa a masą, środek do
wejścia przetwornika analogowo-cyfrowego. Pobór własny 21 µA przy 4,2 V, czyli
0,5 mAh na dobę.

Impedancja źródła wychodzi 50 kΩ, a przetwornik w ATmedze lubi poniżej 10 kΩ,
więc równolegle do dolnego rezystora idzie 100 nF X7R, a program odrzuca
pierwszy odczyt po przełączeniu kanału. Przy większej dokładności warto zejść na
wewnętrzne odniesienie 1,1 V i zmienić dzielnik na 100 kΩ z 33 kΩ, wtedy 4,2 V
daje na wejściu 1,04 V.

## Wykrycie ładowarki

Napięcie z wyjścia odbiornika Qi na wejście cyfrowe.

| Wersja | Rezystor szeregowy | Rezystor do masy | Napięcie na wejściu |
| --- | --- | --- | --- |
| Arduino Nano, 5 V | 10 kΩ | 100 kΩ | 4,5 V |
| XIAO, 3,3 V | 100 kΩ | 150 kΩ | 3,0 V |

Rezystor do masy jest potrzebny nie po to, żeby dzielić napięcie, tylko żeby
wejście miało zdefiniowany stan niski, kiedy ładowarki nie ma. Równolegle
100 nF X7R.

## Linia TX do modułu HC-06

Wyjście Arduino Nano ma 5 V, a wejście RXD modułu pracuje na 3,3 V. Dzielnik
2,2 kΩ w szereg i 3,3 kΩ do masy daje 3,0 V. Prąd dzielnika 0,9 mA, strat 4,5 mW.

Popularny wariant 1 kΩ z 2 kΩ daje 3,33 V, czyli nieco powyżej napięcia
zasilania modułu. Działa, ale bez zapasu, więc lepiej 2,2 kΩ z 3,3 kΩ.

W drugą stronę, z TXD modułu na wejście Nano, nie trzeba nic: 3,3 V mieści się
powyżej progu stanu wysokiego przy zasilaniu 5 V, choć bez dużego zapasu.

## Konwerter poziomów na magistrali I2C

Potrzebny tylko w wersji na Arduino Nano, bo XIAO i czujniki pracują na tym
samym napięciu.

Układ według noty Philipsa AN97055, osobno dla SDA i dla SCL:

| Element | Typ | Liczba |
| --- | --- | --- |
| Tranzystor | BSS138 (SOT-23) albo 2N7000 (TO-92) | 2 |
| Podciągnięcie po stronie 3,3 V | 10 kΩ | 2 |
| Podciągnięcie po stronie 5 V | 10 kΩ | 2 |

Źródło tranzystora idzie na stronę 3,3 V, dren na stronę 5 V, bramka na 3,3 V.
Gotowy moduł czterokanałowy na BSS138 robi to samo i kosztuje mniej niż części
osobno.

Moduł GY-521 ma własne podciągnięcia 4,7 kΩ do 3,3 V, więc po tamtej stronie
rezystory 10 kΩ można pominąć.

## Stabilizator 3,3 V dla czujników

Wyjście 3V3 na płytce Arduino Nano pochodzi z układu USB i jest za słabe.
Osobny stabilizator: MCP1700-3302E/TO w obudowie TO-92, 250 mA, pobór własny
1,6 µA, spadek napięcia 178 mV przy pełnym obciążeniu. Wymaga 1 µF X7R na
wejściu i 1 µF X7R na wyjściu, oba blisko obudowy.

AMS1117-3.3 z gotowych modułów też zadziała, ale pobiera na własne potrzeby
około 5 mA, co przy pracy z ogniwa jest zauważalne.

Jeżeli z tej szyny zasilany jest silnik wibracyjny, dochodzi 100 µF na wyjściu,
zgodnie z rozdziałem o silniku.

## Przetwornica 5 V

Tylko w wersji na Arduino Nano: ogniwo daje 3,7 V, a płytka potrzebuje 5 V na
pin 5V. Gotowy moduł podwyższający (na przykład na MT3608) jest tańszy niż
części osobno. Na jego wyjściu 100 µF elektrolityczny niskoimpedancyjny
i 100 nF X7R przy samym pinie 5V płytki.

## Rezystor programujący prąd ładowania

Moduł TP4056 ustawia prąd ładowania rezystorem między wyprowadzeniem PROG
a masą. Wartości z karty katalogowej: 5 kΩ to 250 mA, 4 kΩ to 300 mA, 2 kΩ to
580 mA, 1,2 kΩ to 900 mA. Fabrycznie moduły mają 1,2 kΩ.

Dla ogniwa 980 mAh rozsądny prąd to 0,3 C, czyli około 300 mA, co daje rezystor
4 kΩ i ładowanie w niecałe cztery godziny. Przy ładowaniu indukcyjnym, które
grzeje ogniwo od strony cewki, warto zejść do 250 mA i rezystora 5 kΩ.

Stała we wzorze różni się między wersjami karty katalogowej (1100 albo 1200),
więc wartości z tabeli są pewniejsze niż wyliczenie, a prąd i tak trzeba
zmierzyć po zmianie rezystora.

## Odsprzęganie

| Miejsce | Kondensator |
| --- | --- |
| przy każdym module i stabilizatorze | 100 nF X7R 50 V |
| wejście i wyjście stabilizatora 3,3 V | 1 µF X7R 16 V |
| szyna 5 V przy płytce Arduino | 10 µF X5R 16 V |
| szyna zasilająca silnik i buzzer | 100 µF elektrolit niskoimpedancyjny 16 V |
| wejście przetwornika analogowo-cyfrowego | 100 nF X7R 50 V |
| wejście wykrywania ładowarki | 100 nF X7R 50 V |

## Zestawienie

Wersja na Arduino Nano z modułem HC-06, poza modułami gotowymi:

| Element | Wartość lub typ | Sztuk |
| --- | --- | --- |
| Rezystor | 1 kΩ | 3 (dioda czerwona, baza silnika, baza buzzera) |
| Rezystor | 680 Ω | 2 (dioda zielona i niebieska) |
| Rezystor | 100 kΩ | 5 (2 × dzielnik baterii, 2 × baza–emiter, 1 × wykrywanie ładowarki) |
| Rezystor | 10 kΩ | 5 (4 × podciągnięcia I2C, 1 × szeregowy do wykrywania ładowarki) |
| Rezystor | 2,2 kΩ | 1 (dzielnik na TX do HC-06) |
| Rezystor | 3,3 kΩ | 1 (dzielnik na TX do HC-06) |
| Rezystor | 22 Ω 0,5 W | 1, tylko gdy silnik zasilany z 5 V |
| Tranzystor | BC337-40 | 2 (silnik, buzzer) |
| Tranzystor | BSS138 albo 2N7000 | 2 (konwerter poziomów I2C) |
| Dioda | 1N4148 | 2 (silnik, buzzer) |
| Kondensator | 100 nF X7R 50 V | 6 |
| Kondensator | 1 µF X7R 16 V | 2 (stabilizator) |
| Kondensator | 10 µF X5R 16 V | 1 |
| Kondensator | 100 µF elektrolit 16 V | 2 (szyna silnika, wyjście przetwornicy) |
| Stabilizator | MCP1700-3302E/TO | 1 |

Wersja na XIAO nRF54L15 różni się trzema rzeczami: zamiast 1 kΩ i 680 Ω przy
diodzie idzie 470 Ω i 100 Ω, odpada konwerter poziomów razem z czterema
rezystorami 10 kΩ i dwoma tranzystorami, odpada przetwornica 5 V razem z jej
kondensatorami. Dzielnik wykrywania ładowarki zmienia się na 100 kΩ z 150 kΩ.
Jeżeli zostaje buzzer 5 V, przetwornica wraca, tylko na potrzeby samego
buzzera.
