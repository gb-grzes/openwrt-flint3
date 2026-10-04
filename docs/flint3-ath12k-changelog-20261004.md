# Flint 3 — poprawki ath12k i połączeń z siecią MLO

Data: 2026-10-04.
Gałąź: `fix-ath12k-legacy-mlo-20261004`.
Kod przetestowany na routerze: `05e29416c8`.
Baza: `4bda6a844a` — działająca wersja z poprawionym raportowaniem mocy Wi-Fi.

## Co zmieniono

- **Obsługa pełnej kolejki sprzętowej TX.** Sterownik sprawdza wolne miejsce
  przed pobraniem ramki z mac80211. Jeśli kolejka jest pełna, ramki pozostają
  w kolejce programowej. Dodano synchronizację wskaźnika sprzętowego i blokadę
  operacji wybudzania dla danego pierścienia TX. To adaptacja upstreamowej
  serii v7, nie gwarancja usunięcia wszystkich zawieszeń firmware Q6.
- **Przygotowanie wyboru pierścienia TX.** Funkcja wyboru przyjmuje kategorię
  ruchu zamiast ramki. Dla QCN9274/IPQ5332 zachowano wybór zależny od CPU;
  dla WCN7850/QCC2072 zachowano wybór według kategorii ruchu.
- **Sprzątanie zasobów po błędzie inicjalizacji AHB.** Przy nieudanej
  konfiguracji IRQ sterownik wyrejestrowuje powiadomienia i zwalnia referencję
  rproc. Prawidłowa ścieżka uruchamiania pozostaje bez zmian.
- **Naprawa połączeń klientów bez MLO z siecią MLO.** Nowa obsługa TX
  początkowo używała pola linku skojarzenia również dla zwykłych klientów.
  Pole pozostawało zerowe, mimo że klient pracował na linku 1 lub 2.
  Sterownik wybiera teraz rzeczywisty link takiego klienta. Naprawia to
  odtworzony błąd blokowania transmisji, obserwowany na Aspire przez 5 GHz.
- **Testy i wersja pakietu.** Rozszerzono modele testowe o rozróżnienie
  klientów MLO i bez MLO. Wersja wydania mac80211 wzrosła z 2 do 4.

## Sprawdzenie

- Wszystkie 235 łatek mac80211 zastosowano do czystych źródeł bez rozluźniania
  dopasowania kontekstu (`fuzz=0`).
- Przeszły 22 grupy testów ath12k z ASan/UBSan, 13 grup testów RTL837x FDB
  i 16 grup testów raportowania mocy iwinfo. Nowy test najpierw odtworzył błąd
  na starej wersji, a po poprawce przeszedł.
- Kompilacja pakietu mac80211 zakończyła się powodzeniem. Pozostały znane
  ostrzeżenia opisane w dokumentacji technicznej; kompilacja nie jest wolna
  od ostrzeżeń.
- Użytkownik zbudował i wgrał obraz. Aspire połączył się z `OpenWrt-MLO`,
  miał adres IPv4 oraz dostęp do routera i Internetu. Oba testy ping
  zakończyły się bez utraty pakietów. Po dodatkowych sprawdzeniach użytkownik
  potwierdził, że wszystko działa. Nie jest to test wielogodzinnego obciążenia.

## Co pozostawiono bez zmian

Kernel **6.18.52**, firmware, konfigurację sieci i DTS. Zachowano wcześniejsze
poprawki LED, 802.11k/CAKE, roamingu/FDB oraz wyświetlania mocy Wi-Fi.
Nie włączono shared RO/MultiPD: nie ustalono dostępności zgodnego kompletu
firmware IPQ5332. Aktualizacja kernela pozostaje osobnym zadaniem.

Szczegóły, źródła poprawek i ograniczenia:
[ath12k-upstream-20261004.md](ath12k-upstream-20261004.md).
