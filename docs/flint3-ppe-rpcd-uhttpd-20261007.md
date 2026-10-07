# Flint 3: poprawka PPE oraz aktualizacje rpcd i uhttpd

Gałąź: `fix-ppe-rpcd-uhttpd-20261007`, utworzona z przetestowanej gałęzi
`fix-mlo-startup-20261004` na `21f88499e28ce774a5e6d7a528ed0e9fe6c0e270`.

Status na 2026-10-07: pełna kompilacja zakończona powodzeniem, obraz wgrany,
użytkownik potwierdził działanie LuCI, internetu, zwykłego 5 GHz oraz MLO.

## Zakres zmian

1. PPE: dodano wyłącznie brakującą poprawkę sprzątania portów po błędzie
   `edma_port_setup()` z [OpenWrt PR #25569](https://github.com/openwrt/openwrt/pull/25569).
   Plik ma numer `0457`, aby następował po lokalnych poprawkach sterownika.
   Treść poprawki i autorstwo Timofeya Maykova zostały zachowane.
   [Źródłowy commit](https://github.com/openwrt/openwrt/commit/bea44e750ff95a996de4810f6721e517b5c49805).
   Kod nie dopuszcza już ujemnego indeksu w pętli sprzątania, zwalnia zegary
   portu, którego inicjalizacja zawiodła, oraz porty EDMA i zegary wcześniej
   uruchomionych portów. Normalna ścieżka uruchamiania pozostaje bez zmian.
2. rpcd: źródła `2026.09.25~a6c6b63b-r1` zastąpiono
   `2026.10.04~d99f703e-r1`. Nazwy plików i katalogów roboczych sesji używają
   osobnego losowego identyfikatora, zamiast ujawniać token sesji.
   [Aktualizacja OpenWrt](https://github.com/openwrt/openwrt/commit/09538fccfcc1ec4d6001512c7136d7aae12a8bc2),
   [poprawka rpcd](https://github.com/openwrt/rpcd/commit/d99f703e4035d03fbcfbada3cb697db346c2b984).
3. uhttpd: źródła `2026.08.03~60f64bec-r1` zastąpiono
   `2026.08.24~373145f7-r1`. Aktualizacja egzekwuje opcjonalne reguły HTTP
   Basic Auth również dla obsługi adresów Lua/ucode/ubus oraz włącza
   `TCP_NODELAY` na przyjętych połączeniach.
   [Aktualizacja OpenWrt](https://github.com/openwrt/openwrt/commit/26410f4160c9bf059aaef275cfba949df5cccb46).
   Dodano także `hexdump -v` w skrypcie startowym, aby powtórzone losowe
   bajty identyfikatora certyfikatu nie były zastępowane znakiem `*`.
   [Poprawka skryptu](https://github.com/openwrt/openwrt/commit/c6c02f7e799e5aea6d2945261ff07cd993ac4ac1).

Pozostałych dwóch zmian z PR #25569 nie nakładano ponownie: ich odpowiedniki
są już obecne w patchach `0410` (zwolniony bufor RX) oraz `0414`/`0415`
(podwójne wyłączenie NAPI i kolejność sprzątania portu Tx).

Nie aktualizowano feedów ani kernela. Kernel pozostaje w wersji **6.18.55**.
Zachowano ath12k, firmware, poprawki MLO/hostapd r5, wifi-scripts, raportowanie
mocy Wi-Fi, LED, fancontrol i tłumaczenia. Konfiguracja `.config` ma niezmieniony
SHA-256: `359f60f1172049d4688f2e75fd78d30b54f022af84ec940097786eed46dc2c60`.
Istniejące niezwiązane modyfikacje wewnątrz feedów pozostawiono bez zmian.

## Sprawdzenie przed pełną kompilacją

- Poprawka PPE nakłada się na dotychczas przygotowany kernel bez odrzuconych
  fragmentów i bez fuzz; przesunięcie o 9 linii wynika z lokalnego stosu patchy.
  Odwrotna próba nakładania potwierdza zgodność testowanej kopii z patchem.
- Test natywny wycina rzeczywistą gałąź błędu i pętle sprzątania z
  `ppe_port.c`, zastępując operacje sprzętowe atrapami. Dla awarii na każdym
  z indeksów 0–5 stary kod wychodzi poza tablicę. Po poprawce **6/6 przypadków
  przechodzi**: zasoby są zwalniane dokładnie raz, we właściwej kolejności,
  bez sprzątania portu EDMA, którego uruchomienie zawiodło. Test korzysta
  z AddressSanitizer i UndefinedBehaviorSanitizer. Nie symuluje całego probe
  ani pozostałych rodzajów awarii i nie jest testem pracy routera.
- Zmieniony `ppe_port.o` oraz cały `qcom-ppe.ko` kompilują się i linkują
  narzędziami AArch64 przeciwko dotychczas przygotowanemu kernelowi 6.18.55.
  Test wykorzystuje osobną kopię sterownika, nie podmienia modułu na routerze
  ani starego kernela w katalogu kompilacji. Modpost zgłasza brak istniejącego
  `MODULE_DESCRIPTION()`; to ostrzeżenie metadanych, nie błąd linkowania.
- `make -j2 package/system/rpcd/compile package/network/services/uhttpd/compile V=s`
  kończy się powodzeniem. Powstają rpcd wraz z wybranymi modułami file,
  iwinfo i ucode oraz uhttpd i uhttpd-mod-ubus w nowych wersjach.
  Występują ostrzeżenia narzędzia ninja o jobserver oraz zależnościach
  niewybranych pakietów feedów; nie występują błędy kompilatora.
- Sumy pobranych archiwów źródeł odpowiadają wartościom z upstream.
  Makefile obu pakietów i skrypt uhttpd są zgodne z porównanym upstream.
- Składnia skryptu uhttpd przechodzi `sh -n`. Cztery powtarzające się bajty
  `0x35` są poprawnie formatowane jako `35353535` przez `hexdump -v`.

Skrypt testowy i logi są zapisane lokalnie w `/home/grzesiek/Documents/Codex/`:

- `ppe-port-unwind-regression-20261007.py`
- `flint3-ppe-port-unwind-before-20261007.log`
- `flint3-ppe-port-unwind-after-20261007.log`
- `flint3-ppe-port-object-build-20261007.log`
- `flint3-ppe-module-build-20261007.log`
- `flint3-rpcd-uhttpd-build-20261007.log`

## Pełna kompilacja i sprawdzenie na routerze

Użytkownik zbudował i wgrał obraz z commita
`41074a954cb55480c95661bf188cdec0bedb663e`. Log pełnej kompilacji:
`/home/grzesiek/Documents/Codex/flint3-ppe-rpcd-uhttpd-full-build-20261007.log`.
Nie znaleziono błędów kompilacji ani odrzuconych fragmentów patchy; poprawka
`0457` została zastosowana bez fuzz. W logu występują ostrzeżenia narzędzi
budowania, nagłówków i innych pakietów, ale pełna kompilacja kończy się
utworzeniem obrazów, manifestu, indeksów i sum kontrolnych.

SHA-256 obrazu `openwrt-qualcommbe-ipq53xx-glinet_gl-be9300-squashfs-sysupgrade.bin`:
`8b48e3c78e5cddd0728bb07006eb5d6f52bb857a9258b07af8eacd59dd8609d2`.
Wszystkie osiem pozycji z `sha256sums` przechodzi sprawdzenie.
Manifest zawiera kernel 6.18.55, rpcd `2026.10.04~d99f703e-r1`, uhttpd
`2026.08.24~373145f7-r1`, hostapd r5, fancontrol 2.0.0-r1 i jego LuCI 2.0.0-r6.
Binarne rpcd, uhttpd i spakowany moduł PPE wewnątrz obrazu są zgodne z nowo
zbudowanymi pakietami. Wypełnienie za systemem plików SquashFS jest oczekiwane.

Wynik `ubus call system board` na routerze potwierdza Flint 3, kernel 6.18.55
oraz rewizję `r0+36566-41074a954c`. Dostarczony wycinek logu pokazuje:

- `ppe_offload: ipv6 self-test passed`, `EDMA Hardware Configured`
  i `EDMA configuration successful`;
- wszystkie trzy zwykłe interfejsy AP osiągają `AP-ENABLED`;
- brak wcześniejszego błędu dodawania łącza MLO i wyjścia hostapd z signal 11
  w dostarczonym wycinku.

`iw dev` potwierdza pasma 2,4/5/6 GHz oraz oba łącza `ap-mld0`:
5 GHz na kanale 36, 80 MHz, adres `96:83:c4:ce:a3:f8`; 6 GHz na kanale 21,
320 MHz, osobny adres `00:03:7f:12:62:82`. Poprzednia poprawka rozdzielenia
adresów MLO pozostaje skuteczna. Użytkownik następnie potwierdził, że wszystko
działa, w odpowiedzi na pytanie o logowanie do LuCI oraz internet przez zwykłe
5 GHz i MLO.

To potwierdza prawidłowy start i podstawowe działanie na routerze. Nie jest to
test długotrwałego obciążenia ani celowo wywołanej awarii inicjalizacji PPE.
Późniejszy commit aktualizujący wyłącznie ten opis nie wymaga nowej kompilacji.

## Kompilacja pełnego obrazu

```sh
cd /home/grzesiek/openwrt-flint3
git switch fix-ppe-rpcd-uhttpd-20261007
set -o pipefail
make -j"$(nproc)" V=s 2>&1 | tee /home/grzesiek/Documents/Codex/flint3-ppe-rpcd-uhttpd-full-build-20261007.log
```

Nowa poprawka kernela powoduje automatyczne przygotowanie źródeł na nowo.
Nie trzeba ręcznie usuwać katalogów ani wykonywać `make clean`.
Po kompilacji warto potwierdzić nowe wersje rpcd i uhttpd w manifeście.

Po wgraniu nowego obrazu sprawdzić uruchomienie LAN/WAN, logowanie do LuCI,
zapis ustawień, zwykłe 5 GHz oraz dostęp do routera i internetu przez MLO.
Przy pierwszej weryfikacji zachować dotychczasowe ustawienia Wi-Fi.
Nie wywoływać celowo awarii inicjalizacji PPE na używanym routerze.

Docelowa gałąź na GitHub:
[`fix-ppe-rpcd-uhttpd-20261007`](https://github.com/gb-grzes/openwrt-flint3/tree/fix-ppe-rpcd-uhttpd-20261007).
Publikacja dotyczy osobnej gałęzi; gałąź główna pozostaje bez zmian.
Nie tworzono PR.
