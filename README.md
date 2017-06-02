# MPI_PayedKillers

W pewnym dużym mieście w odległej przyszłości istnieje N firm oferujących usługi upraszczające relacje w pracy i rodzinie. Każda firma ma osobną reputację. Procesy reprezentują stałych klientów tych firm, którzy co pewien czas ubiegają się o dostęp do jednej z nich. Każda firma ma od 1 do Z zabójców na stanie. Procesy ubiegają się zawsze o jednego zabójcę, usiłując zawsze wybrać najlepszą firmę, a jeżeli ta ma akurat wszystkich zabójców zajętych, wybrać kolejną z nich - uwaga, procesy nie odpytują firm, raczej cały czas próbują się skolejkować u jednej z nich, a wybierają po prostu najlepszą aktualnie dostępną ofertę. Jeżeli proces widzi, że w jakiejś kolejce o wyższej reputacji jest już blisko, powinien zrezygnować z dostępnego miejsca w kolejce o niższej reputacji. Po skorzystaniu z usług firmy procesy (losowo) wyrażają opinię o firmie. Wpływa to na zmianę jej reputacji.

##Inicjalizacja:
Maszyna o identyfikatorze równym 0 losuje odpowiednią liczbę zabójców dla poszczególnych firm oraz ich reputację startową. Następnie wysyła te informacje do innych procesów. Pozostałe maszyny czekają na informację, po otrzymaniu której mogą rezerwować zabójców.

##Informacja:
Informacje dotyczące rezerwacji/zwolnienia zabójcy oraz zmiany reputacji firmy składają się z:
id procesu który wysyła informację
numer firmy
rodzaj informacji
znacznik czasowy (unix timestamp)

##Zasoby procesu:
tablicę reputacji firm
N kolejek (dla każdej z firm), każdy wpis w kolejce składa się z id klienta oraz znacznika czasowego
liczbę zabójców dla każdej firmy

##Opis algorytmu:
- Proces Pj , który chce wynająć zabójcę znajduje firmę Fi o największej reputacji i wysyła informację REQ do każdego innego procesu Pi ze swoim znacznikiem czasowym. W wiadomości zawarty jest też numer firmy Fi, do której klient chce się zakolejkować.
- Kolejny proces Pi otrzymując wiadomość dopisuje proces Pj do kolejki na podstawie znaczników czasowych (jeżeli są takie same to na podstawie id klienta). Proces Pi następnie wysyła odpowiedź OK zawierającą swój znacznik czasowy.
- Proces Pj wie, że dostał zabójcę jeżeli otrzymał od wszystkich innych procesorów Pi potwierdzenie OK ze znacznikami czasowymi większymi od jego zapytania oraz znajduje się wśród Z (liczba zabójców) najstarszych procesów w kolejce.
- Jeżeli proces Pj nie jest w Z najstarszych, ponawia zapytania do kolejnej firmy Fj w zależności od reputacji.
- Jeżeli proces Pj dostał zabójcę a był zakolejkowany w innych firmach wysyła do innych procesów Pi komunikat REL zwolnienia z tych kolejek.
- Proces Pj po wykorzystaniu zabójcy wysyła informację o zwolnieniu  REL do każdego z procesów Pi oraz informację RANK odnośnie zmiany reputacji firmy (wartość nie określa reputacji tylko jej zmianę np 10 lub -5)
- Jeżeli proces Pj nie dostał zabójcy i czeka w kolejkach oraz dostanie informację REL (o zwolnieniu którejś z kolejek) to sprawdza czy w którejś kolejce o większej reputacji jest bliżej niż w kolejkach o mniejszej reputacji. Jeżeli tak to wysyłą informację REL odnośnie zwolnienia z tychże kolejek.
