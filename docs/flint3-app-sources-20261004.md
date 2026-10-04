# Flint 3 — aktualizacja źródeł aplikacji

Data aktualizacji: 2026-10-04.
Gałąź: `update-app-sources-20261004`.
Baza: `cd5f2348ab99081aa6e1a8db65a9ed6016b09604`,
gałąź `update-kernel-6.18.55-20261004`.

## Zakres i zachowanie zmian

Zaktualizowano feedy oraz aplikacje systemowe do wersji dostępnych w
oficjalnych źródłach OpenWrt podczas przygotowania tej gałęzi. Nie jest
to aktualizacja wszystkich programów do niezależnych wydań ich autorów.
Punkt odniesienia głównego drzewa OpenWrt:
[`195c2ce324`](https://github.com/openwrt/openwrt/commit/195c2ce324930740fa6b4aa898c999e5889d2c11).

Kernel pozostaje **6.18.55**. Katalogi `target/linux`, `package/kernel`
i `package/firmware` nie zostały zmienione względem bazy.
Zachowano lokalne poprawki ath12k, RTL837x/FDB, raportowania mocy,
LED, 802.11k/CAKE i wyłączania 802.11r na AP MLO.
Nie zmieniano konfiguracji sieci działającego routera i niczego na niego
nie wgrywano. Potwierdzenie działania poprzedniej gałęzi na sprzęcie nie
jest potwierdzeniem działania tego zestawu nowych aplikacji.

## Najważniejsze aktualizacje

| Element | Stan w tej gałęzi |
|---|---|
| OpenSSL | 3.5.9 |
| PCRE2 | 10.49, wydanie pakietu 2 |
| libpcap / tcpdump | 1.10.7 / 4.99.7 |
| elfutils | 0.196 |
| netifd | źródła z 2026-09-11 |
| procd | źródła z 2026-09-27 |
| rpcd, ubus, udebug | źródła z 2026-09-25 |

Dnsmasq 2.93 i Dropbear 2026.94 pozostały bez zmiany wersji: w
sprawdzonym zakresie oficjalnego drzewa nie było ich kolejnej aktualizacji.

Hostapd zachowuje źródła bazowe z 2026-08-07 i lokalny zestaw łatek,
uzupełniony oficjalnymi zmianami OpenWrt. Obejmują one m.in. zachowanie
interfejsów i BSS-ów MLD podczas zmiany konfiguracji, adres linku radio 0,
obsługę zmiany nazwy BSS, DPP, MBO/BTM, referencje obiektów ubus oraz
poprawki APuP. Skrypty Wi-Fi i LuCI wymuszają zgodne tryby WPA3 na 6 GHz.
Nie deklarujemy usunięcia przyczyny wszystkich prób ponownego startu MLO;
wymagany jest test na routerze. Lokalna blokada 802.11r na MLO pozostaje.

Zaimportowano 56 commitów dotyczących aplikacji oraz 7 wymaganych
poprawek narzędzi budowania: ATOMIC64, obsługa równoległych wariantów,
oddzielne katalogi staging, niedublowanie wariantów, zachowanie zależności
rzeczywistych pakietów i ich warunków oraz usunięcie pętli zależności Kconfig.
Zachowano wcześniejsze lokalne dostosowania narzędzi budowania.

## Przypięte feedy

Dokładne rewizje zapisano w `feeds.conf.default`:

| Feed | Rewizja |
|---|---|
| packages | `dee4f5f48defb47580df932ec1c8ab9244bf57d4` |
| luci | `aa3d48836e90ae0706c8d8f9b46b8371e45cfe1f` |
| routing | `4b9891b9136259f93294a424507ed24c5e8c1cbd` |
| telephony | `5d68d53c160a325ea9d03fce393e051573bcc736` |
| video | `afb0a7453e4ecebe4b59f9c1b5f8d5413fb652f2` |

Packages przesunięto o 250 commitów, LuCI o 77, video o 32.
Routing i telephony były już na aktualnych rewizjach.
Źródła: [packages](https://github.com/openwrt/packages/commit/dee4f5f48defb47580df932ec1c8ab9244bf57d4),
[LuCI](https://github.com/openwrt/luci/commit/aa3d48836e90ae0706c8d8f9b46b8371e45cfe1f),
[video](https://github.com/openwrt/video/commit/afb0a7453e4ecebe4b59f9c1b5f8d5413fb652f2).

## Fancontrol i lokalne poprawki feedów

Kod kontrolera `package/fancontrol`, jego ustawienia, skrypty uruchomieniowe
oraz kod JavaScript i wygląd panelu `package/luci-app-fancontrol` zachowano
bez zmian względem bazy.

Dodano polskie tłumaczenie:
- `Controls when the fan starts and how its speed rises with temperature.`
- „Określa, kiedy wentylator się uruchamia i jak jego prędkość rośnie wraz z temperaturą.”

Usunięto również dwa osierocone, zdublowane wpisy `msgstr` w polskim PO,
które uniemożliwiały ścisłą walidację katalogu. Prawidłowe tłumaczenia tych
etykiet pozostają. Wydanie `luci-app-fancontrol` zwiększono z 5 do 6,
aby przebudować także pakiet tłumaczenia.

W głównym repo zapisano:
- `patches/feeds/luci/001-preserve-babeld-removal.patch` — zachowuje
  wcześniejsze lokalne usunięcie 25 plików luci-app-babeld;
- `patches/feeds/packages/001-preserve-libevdev-dependencies.patch` —
  zachowuje poprzedni brak zależności libevdev od nowego input-support;
  źródła libevdev pozostają 1.13.6, lokalne wydanie pakietu wynosi 3.

Nowe feedy korzystają z opcjonalnych pakietów hardware-support. Ich
dodanie oznaczałoby nowe grupy i reguły uprawnień urządzeń; szeroki import
został odrzucony przez automatyczną kontrolę bezpieczeństwa i nie został
wykonany. Zastosowano zgodność z dotychczasowym libevdev zamiast rozszerzać
uprawnienia. Pozostałe pakiety wymagające tych nowych funkcji są niewybrane
i mogą powodować ostrzeżenia o brakujących opcjonalnych zależnościach.
BMX7 również nie ma definicji w aktualnym przypiętym routing; jego
niewybrane aplikacje zgłaszają ostrzeżenia.

Nie importowano migracji sterowników audio/display/input do nowych
bramek funkcji ani tworzenia partycji provisioning podczas sysupgrade.
Dotychczasowy sposób aktualizowania routera pozostaje bez zmian.

## Przygotowanie i kompilacja

W tym checkoutcie:

```sh
cd /home/grzesiek/openwrt-flint3
git switch update-app-sources-20261004
sh scripts/flint3-feeds-prepare.sh
make defconfig
make -j"$(nproc)"
```

Skrypt przygotowania pobiera wyłącznie brakujące feedy, sprawdza przypięte
rewizje, odtwarza zapisane poprawki i odświeża indeksy oraz linki pakietów.
Nie uruchamiaj przygotowania feedów równocześnie z kompilacją w tym samym
checkoutcie — oba etapy korzystają ze wspólnych indeksów tymczasowych.
Nie resetuje ani nie przełącza istniejących feedów. Gdy mają inne rewizje,
zatrzymuje się i prosi o świadome przygotowanie źródeł. Ponowne wykonanie
jest bezpieczne dla już zastosowanych zapisanych łatek.
Zwykłe `scripts/feeds update -a` może usunąć i ponownie sklonować checkout
przy zmianie zapisanej konfiguracji URL; używaj powyższego skryptu.

**Feedy i .config są wspólne dla gałęzi w jednym checkoutcie.** Samo
przełączenie gałęzi nie przywraca wersji pakietów poprzedniego obrazu.
Przed zmianą feedów zachowano lokalną kopię konfiguracji, przypięć obrazu
i wcześniejszej poprawki LuCI; kopia nie jest publikowana w repo.

## Weryfikacja

Polski katalog przechodzi `msgfmt --check` oraz kompilację `po2lmo`.
Pakiety `luci-app-fancontrol` i `luci-i18n-fancontrol-pl` skompilowano;
nowe tłumaczenie jest obecne w wygenerowanym pliku LMO.

Porównanie konfiguracji potwierdza zachowanie wszystkich **308** wcześniej
wybranych wpisów `CONFIG_PACKAGE_*=y/m`. Zmieniły się automatycznie wykrywana
cecha HAS_ATOMIC64 i nowa domyślna funkcja odwrotnego wyszukiwania historii
BusyBox. Nie włączono nowych pakietów hardware-support.

Testy zachowanych poprawek przechodzą: 16 grup raportowania mocy iwinfo,
13 grup RTL837x/FDB i 22 grup ath12k, z ASan/UBSan poza ograniczeniami
środowiska sandbox. Powtórzone sprawdzenie składni czterech skryptów ucode
zwróciło kod 0 dla każdego pliku: hostapd, wpa_supplicant i mac80211 w trybie
programu, wifi/ap w trybie modułu. Wcześniejsze próby użycia niewłaściwego
trybu kompilatora były odrzucone; nie były błędami poprawnie wybranego trybu.
Moduły zależne pozostają linkowane dynamicznie — to kontrola składni,
nie test wykonania ani uruchomienia Wi-Fi.

Konfiguracja nadal zgłasza dwie pętle zależności w **niewybranych** opcjonalnych
pakietach: wybór OpenGL w Qt5 oraz SQUEEZELITE_WMA_ALAC w squeezelite-custom.
`make defconfig` zapisuje konfigurację i zwraca kod 0, a sprawdzona kompilacja
wybranych pakietów zakończyła się powodzeniem. Nie oznacza to jednak walidacji wszystkich opcji menu
bez błędów. Pozostały także opisane wyżej ostrzeżenia o opcjonalnych
zależnościach; aktualizacja nie jest pozbawiona komunikatów diagnostycznych.

Sprawdzenie kompilacji zakończyło się **kodem 0** dla OpenSSL, PCRE2,
procd, ubus, rpcd, netifd, wifi-scripts, hostapd full-openssl,
wpad full-mbedtls i panelu fancontrol z tłumaczeniem. Zbudowano również
wymagane zależności, w tym udebug, odhcpd-ipv6only, ncurses i mtd-utils.
W logu tej kompilacji nie ma nieudanych fragmentów łatek ani użycia fuzz.
Sprawdzenie dotyczy wybranych najważniejszych pakietów, nie całego feedu.

Wynikowy panel to `luci-app-fancontrol-2.0.0-r6.apk`, a nowe polskie
tłumaczenie ma wersję `26.277.55857~02de966`. Nie należy wybierać starszego
pliku tłumaczenia o wersji `26.253.63371~733cc86`, jeśli pozostał w katalogu
pakietów po poprzedniej kompilacji.

Pełny obraz tej gałęzi nie został jeszcze zbudowany ani przetestowany na routerze.
Obrazy pozostałe w `bin/targets` po poprzedniej kompilacji nie zawierają
tej aktualizacji aplikacji — przed wgraniem trzeba zbudować nowy obraz.
