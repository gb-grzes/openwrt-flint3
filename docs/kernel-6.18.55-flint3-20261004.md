# Flint 3 — aktualizacja kernela do 6.18.55

Data: 2026-10-04.
Gałąź: `update-kernel-6.18.55-20261004`.
Baza: `28a2f3e568` z gałęzi `fix-ath12k-legacy-mlo-20261004` — kod ath12k
potwierdzony przez użytkownika na routerze, wraz z opisem testów.

## Zakres

Aktualizacja **6.18.52 → 6.18.55**, w obrębie dotychczasowej serii LTS.
[kernel.org](https://www.kernel.org/) wskazywał 6.18.55 jako najnowsze wydanie
tej serii podczas przygotowania aktualizacji. Nie jest to przejście na 7.x.

Włączono oficjalne dostosowania OpenWrt dla
[6.18.53](https://github.com/openwrt/openwrt/commit/411a8112dd1211b47469fd99dc9c0d25560f5b1c)
i [6.18.54](https://github.com/openwrt/openwrt/commit/142619bac5ba56d5c8adbb80b6a65095057e0a65),
w tym usunięcie łatek już obecnych w stabilnym kernelu oraz dostosowanie
pozostałych do nowych źródeł. Lokalnie odpowiadają im commity `18742e63c7`
i `878362636c`. Następnie zmieniono wersję i sumę archiwum na 6.18.55.

Konflikty importu dotyczyły również wcześniejszych dostosowań MediaTek i
Raspberry Pi. Dla wspólnych łatek MediaTek wykorzystano oficjalne rebazy
6.18.53, odpowiadające zmianom stabilnych źródeł. Nie przywrócono trzech
łatek Raspberry Pi, które w forku były już usunięte, i zachowano lokalny
kontekst istniejącej łatki xHCI. Zestaw aktualizacji zawiera więc także
oficjalne dostosowania innych platform, ale sprawdzenie kompilacji dotyczy
wyłącznie **qualcommbe/ipq53xx, GL-BE9300 (Flint 3)**.

Usunięte dublujące backporty i wcześniejsze wersje łatek pozostają dostępne
w historii Git. Nie usunięto poprawek specyficznych dla Flint 3.

## Istotne zmiany

- Poprawki czasu życia obiektów multicast w moście sieciowym oraz usuwania
  wpisów MDB. Kod mostu z IGMP snooping jest włączony w tym obrazie.
- Poprawki czasu życia conntrack/flow offload: zwolnienie referencji po
  zakończeniu okresu RCU oraz publikacja stanu HW_DEAD dopiero po ostatnim
  dostępie workera do przepływu. Zależność od rzeczywistego użycia offloadu
  pozostaje istotna; to nie jest strojenie FIFO PPE.
- Walidacja rozmiaru słownika XZ w SquashFS. Chroni obsługę niepoprawnych
  obrazów; nie stanowi przyspieszenia normalnego startu routera.

Źródła: [6.18.53](https://cdn.kernel.org/pub/linux/kernel/v6.x/ChangeLog-6.18.53),
[6.18.54](https://cdn.kernel.org/pub/linux/kernel/v6.x/ChangeLog-6.18.54),
[6.18.55](https://cdn.kernel.org/pub/linux/kernel/v6.x/ChangeLog-6.18.55).
Obecność wymienionych zmian sprawdzono również w przygotowanym, załatanym
kodzie 6.18.55, a nie tylko na podstawie tytułów commitów.

## Zachowane elementy

Bez zmian pozostają firmware, backports 7.2 i mac80211 wydanie 4, sterownik
RTL837x, poprawki iwinfo, konfiguracja sieci i źródła platformy qualcommbe.
Zachowano LED, 802.11k/CAKE, blokadę 802.11r na AP MLO, poprawkę roamingu/FDB,
wyświetlanie mocy oraz poprawiony wybór linku dla klientów bez MLO.
Nie włączono shared RO/MultiPD.

Wi-Fi nadal pochodzi z pakietu mac80211/backports 7.2. Zmiana kernela nie
zastępuje tego sterownika jego wersją z podstawowego drzewa Linux 6.18.55.
Nie deklarujemy usunięcia zawieszeń Q6, wszystkich ostrzeżeń PCIe/MLO ani
przepełnień FIFO PPE.

## Weryfikacja

- Archiwum `linux-6.18.55.tar.xz` pobrano z kernel.org i sprawdzono względem
  [opublikowanej sumy SHA-256](https://cdn.kernel.org/pub/linux/kernel/v6.x/sha256sums.asc):
  `f410638061a165c12f42ab871d2f3fcd525515359b5faeee80969cff84524df9`.
- `make target/linux/prepare PATCH='patch --fuzz=0' V=s` zakończyło się
  powodzeniem. Zastosowano wszystkie **598 łatek** generic i qualcommbe.
  Przesunięcia numerów linii są dopuszczalne; fuzz nie był dopuszczony.
- `.config` OpenWrt pozostał bez zmian, SHA-256:
  `a0f5d31055c39c930129f53eba0579f69094ecfdfb58c5466220d9716ea4f999`.
  Porównanie wygenerowanej konfiguracji kernela z poprzednim obrazem wykazało
  tylko opis wersji oraz różnice etapu initramfs, bez zmiany wyboru funkcji.
- `make target/linux/compile -j4 V=s` zakończyło się kodem 0. Kernel ma
  `kernel.release=6.18.55` i architekturę AArch64. Zbudowano również moduły
  natywne, w tym PPE, flowtable i CAKE.
- `make package/kernel/mac80211/compile package/kernel/rtl837x/compile
  package/kernel/gpio-button-hotplug/compile -j4 V=s` zakończyło się kodem 0.
  Sprawdzono `vermagic=6.18.55 SMP mod_unload aarch64` dla ath12k_wifi7,
  RTL837x, przycisków, PPE, flowtable i CAKE. Nowe pakiety wymagają kernela
  `6.18.55~0ecb32f4af418a46d89d32d72665b21e-r1`; nie są pakietami dla 6.18.52.
- Wszystkie 22 grupy testów ath12k przechodzą z ASan/UBSan również na źródłach
  przygotowanych i skompilowanych dla 6.18.55. Siedem plików sterownika
  odpowiada wcześniejszemu, sprawdzonemu drzewu backports 7.2 bajt w bajt.
  Poprawiony callback TX znajduje się w ath12k_wifi7, a wymagany symbol
  `ath12k_hal_srng_src_num_free` jest eksportowany przez ath12k.
- Ponownie przeszły 13 grup testów RTL837x FDB i 16 grup testów mocy iwinfo.
- Pozostały ostrzeżenia MODULE_DESCRIPTION, jobserver, zależności Kconfig
  QCOM_MDT_LOADER oraz pustych opcjonalnych pakietów crypto-kpp/fs-netfs.
  Nie zmieniano tych ustawień w ramach aktualizacji. Kompilacja nie jest
  wolna od ostrzeżeń, ale zakończyła się bez błędów kompilacji i linkowania.
- Użytkownik potwierdził pełną kompilację obrazu bez błędów. Sprawdzono
  sumy plików względem `sha256sums`, manifest pakietów, `profiles.json`
  oraz metadane osadzone w sysupgrade: GL-BE9300, qualcommbe/ipq53xx,
  kernel 6.18.55, rewizja źródeł `299edbf297`.

## Uruchomienie na routerze

Użytkownik wgrał sysupgrade i dostarczył log kernela oraz późniejsze
`iw dev` i filtrowane `logread`. Sprawdzony obraz:
`openwrt-qualcommbe-ipq53xx-glinet_gl-be9300-squashfs-sysupgrade.bin`.
SHA-256:
`d16fa977202287c71b4aa4eb42b23673c2eeb41224aeb2e1c3aa5325dc485a77`.
Zmiany dokumentacyjne wykonane po tej kompilacji nie zmieniają kodu firmware.

- Log potwierdza Linux 6.18.55 na GL.iNet GL-BE9300 oraz poprawne
  zamontowanie SquashFS i zapisywalnego overlay F2FS.
- PPE/EDMA i RTL837x zostały zainicjalizowane. Połączenie wewnętrzne
  SoC–switch osiągnęło 10 Gb/s, a fizyczny LAN2 2,5 Gb/s. Nie jest to pomiar
  przepustowości ani potwierdzenie działania ścieżki WAN.
- Firmware Wi-Fi pozostał bez zmian: IPQ5332 1.6-01270 i QCN9274 1.6-01243.
  W dostarczonym fragmencie nie ma panic/oops, błędu ładowania modułów
  ani awarii firmware Q6.
- `iw dev` pokazuje trzy AP: 2,4 GHz, kanał 7, 20 MHz, 16 dBm;
  5 GHz, kanał 36, 80 MHz, 22 dBm; 6 GHz, kanał 21, 320 MHz, 22 dBm.
  Przejście 2,4 GHz na 20 MHz jest jawnie opisane przez hostapd jako skutek
  wykrytych sąsiednich BSS, a nie nieudanego startu radia.
- `ap-mld0` ma link 1 na 5 GHz i link 2 na 6 GHz. Aspire oraz drugi klient
  zakończyły uwierzytelnianie SAE i uzgadnianie kluczy, co potwierdzają
  `AP-STA-CONNECTED` i `EAPOL-4WAY-HS-COMPLETED` o 16:15:56 i 16:18:11.
  Same te wpisy nie potwierdzają DHCP ani dostępu do Internetu.

**Pozostałe ograniczenia:** przy starcie hostapd kilkukrotnie zgłasza
`MLD: Failed to add link 1 in MLD ap-mld0`, po czym interfejsy Wi-Fi są
odtwarzane. Końcowe uruchomienie wszystkich AP następuje około 60. sekundy;
w późniejszym, dostarczonym fragmencie nie ma dalszych restartów. Podobne
ponawianie startu występowało już na 6.18.52. Dokładnej przyczyny nie ustalono;
nie przypisujemy jej automatycznie nowemu kernelowi i nie uznajemy jej za
naprawioną. Ostrzeżenia o nakładaniu pamięci WCSS, początkowym resecie MMC,
zapasowej GPT i STP portów 0–2 również były obecne w poprzednim logu.

To potwierdzenie startu i podstawowej inicjalizacji, nie test stabilności
wielogodzinnej ani pełnej łączności klientów. W dostarczonych wynikach dla
6.18.55 nie ma jeszcze testów ping/DHCP, roamingu ani obciążenia.

Logi znajdują się w `/home/grzesiek/Documents/Codex/` pod nazwami
`kernel-6.18.55-prepare-20261004.log` i
`kernel-6.18.55-compile-20261004.log`,
`kernel-6.18.55-modules-20261004.log`,
`kernel-6.18.55-ath12k-host-test-prepared-20261004.log`,
`kernel-6.18.55-fdb-host-test-20261004.log` i
`kernel-6.18.55-txpower-host-test-20261004.log`.

