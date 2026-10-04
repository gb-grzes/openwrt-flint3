# Flint 3: failed MLO link setup, startup crashes and duplicate link addresses

Branch: `fix-mlo-startup-20261004`, based on `update-app-sources-20261004`
at `71d059856e526c031acdf8a9d805348b0f10916a`.

## Podsumowanie zmian

Naprawa startu Wi-Fi/MLO na Flint 3: zabezpieczenie sprzątania po nieudanym
dodaniu łącza, dokładniejszy opis błędu oraz zachowanie osobnych adresów MAC
dla łączy 5 i 6 GHz. Usunięto konflikt, przez który start 6 GHz przed 5 GHz
powodował błąd `-114` i wyłączenie całego radia 5 GHz. Hostapd ma wydanie `r5`.
Kompilacja i po 20 testów regresji dla obu wariantów zakończyły się powodzeniem.
Po wgraniu użytkownik potwierdził działanie wszystkich pasm oraz dostęp do
internetu przez zwykłe 5 GHz i MLO. Kernel, firmware, ustawienia Wi-Fi,
fancontrol i tłumaczenia pozostały bez zmian.

## First iteration: crash evidence and scope

The supplied boot logs contain six sequences of:

```text
MLD: Failed to add link 1 in MLD ap-mld0
jail (...) exited with signal: 11
```

Wi-Fi eventually starts around 57 seconds after boot. Clients subsequently
complete SAE/RSN authentication on ordinary 5 GHz and the MLO SSID. The user
also confirms internet access. Earlier boot logs already contain repeated
MLD link-add failures and radio recreation; the application update alone is
therefore not established as their cause.

There is no driver error number or userspace crash backtrace in these logs.
They do not establish why the driver rejects the link or prove which exact
instruction crashed the router's hostapd process.

## First iteration: changes

1. `602-AP-MLD-clean-up-only-registered-links.patch`: base unlinking on actual
   list membership, not `started`, first-BSS position or a driver pointer.
   `hostapd_setup_bss()` sets `started` before calling driver `link_add`.
   A failed second link can therefore be marked started without being on the
   MLD list; the previous cleanup passes a NULL node to `dl_list_del()`.
   The native regression test reproduces this NULL dereference on the
   unmodified code. Removal is also safe when called twice, and registered
   links are unlinked even if driver private data was already cleared.
2. The failed dynamic BSS-add path in `src/src/ap/ucode.c` unlinks any registered
   MLD link before freeing its BSS, releases idempotent ucode/ubus resources,
   drops the MLD reference and collects unused MLDs. This avoids dangling
   list entries and references when setup fails after registration.
3. `603-AP-MLD-report-link-add-errors.patch`: log interface, link ID, link MAC
   and the driver's return code at error level. Both initial and dynamic
   setup use this wrapper. Successful setup is unchanged and stays quiet.
4. Increase hostapd package release from 3 to 4 so all matching packages are
   rebuilt together.

This is a crash-safety fix and diagnostic build, not yet a demonstrated fix
for the underlying driver rejection. No arbitrary delay, forced restart,
country/channel/encryption change, or MLO disabling is added.

Kernel 6.18.55, ath12k, firmware, Wi-Fi scripts, fancontrol, translations and
the application-source update remain unchanged. `.config` retains SHA-256
`359f60f1172049d4688f2e75fd78d30b54f022af84ec940097786eed46dc2c60`.

## First iteration: validation

- Before correction: three selected tests fail, including NULL list-node
  access after a failed second link and incorrect cleanup after driver data
  was cleared. Log:
  `/home/grzesiek/Documents/Codex/hostapd-mlo-startup-before-20261004.log`.
- After correction: 14/14 tests pass on both prepared source variants with
  AddressSanitizer, UndefinedBehaviorSanitizer and leak detection. The tests
  extract the actual cleanup functions, link-add wrapper and dynamic failure
  cleanup block. BSS, driver and resource APIs are stubbed; no radio is run.
  Logs: `hostapd-mlo-startup-after-20261004.log` and
  `hostapd-mlo-startup-wpad-after-20261004.log` in the same directory.
- `make -j2 package/network/services/hostapd/compile V=s` succeeds, building
  hostapd/full-openssl and wpad/full-mbedtls and their release-4 APKs.
  Build log: `/tmp/flint3-mlo-startup-build-20261004.log`.
- Patches apply without failed hunks or fuzz. Known build parallelism warnings
  (`jobserver unavailable` / ninja ignoring jobserver) are present.

The user subsequently built and flashed the release-4 image at commit
`de6f95be2b9dfa06c77b8dd059a4eed56f21fb55`. Its sysupgrade SHA-256 is
`96ce3780bfdf8f561ef089389393db8b446f77f8d8028b46904a4c724586602b`.
The full-build log is
`/home/grzesiek/Documents/Codex/flint3-mlo-startup-full-build-20261004.log`.
The next boot log contains no signal-11 exit, but ordinary 5 GHz remains
disabled and MLO has only its 6 GHz link. Crash prevention did not fix the
underlying link-add failure; the additional diagnostic now identifies it.

The previously working application-update image is saved
with its manifest and profile metadata in
`/home/grzesiek/Documents/Codex/flint3-before-mlo-startup-20261004.OgvAJb`.
Its sysupgrade SHA-256 is
`4ed2d9e3001dafd6dd8f82f09522710b58e5025950d066c53558758f19739d80`.

## Second iteration: duplicate first-link address

The new log is
`/home/grzesiek/.codex/attachments/e3c71a1a-3cca-4d30-9e20-94715378a909/Wklejony tekst.txt`.
It shows these events:

```text
radio1: Preparing interface ap-mld0 with MAC: 96:83:c4:ce:a3:f8
radio2: Preparing interface ap-mld0 with MAC: 00:03:7f:12:c3:e3
iw dev: link ID 2 link addr 96:83:c4:ce:a3:f8
MLD: link_add failed: ifname=ap-mld0 link_id=1 addr=96:83:c4:ce:a3:f8 ret=-114 (Operation already in progress)
phy0.1-ap0: AP-DISABLED
```

The configured link addresses are different, but the actual first link
(6 GHz, which finishes setup before the 5 GHz HT scan) uses the shared MLD
netdev address. The later 5 GHz link requests that same address.
`ieee80211_check_dup_link_addrs()` in the prepared mac80211 `net/mac80211/link.c`
returns `-EALREADY` for duplicate per-link addresses, before the ath12k link
change callback. The failure also aborts the ordinary 5 GHz BSS setup.

In `hostapd_setup_bss()`, the first MLD link on a secondary BSS passes a NULL
address to `hostapd_if_add()` to reuse the shared netdev. Its returned address
overwrites `hapd->own_addr`, including the already selected BSSID. Existing
links skip this block and keep their configured BSSID; the result depends on
which band finishes first.

`604-AP-MLD-preserve-first-link-BSSID.patch` saves the netdev address in
`mld->mld_addr`, then restores the configured BSSID to the link's `own_addr`.
For an unconfigured BSSID, it mirrors the random-link-address fallback used
by `hostapd_driver_init()` in `hostapd/main.c`. The patch leaves later MLD
links and non-MLO BSS address handling unchanged. Hostapd package release
increases from 4 to 5; all first-iteration crash-safety changes remain.

### Second iteration: regression tests

Six address tests extend the existing 14 cleanup/diagnostic tests. The harness
extracts the actual first-link address block from the prepared C source and
uses the real `is_zero_ether_addr()` helper. The random-address fallback is a
deterministic stub, so these tests do not assess randomness quality or radio
operation. The extractor also excludes comment references to function names.

Before the address fix, three of four selected tests fail: configured BSSID
preservation, fallback address separation, and the observed 6 GHz-first order.
The 5 GHz-first order passes. Log:
`/home/grzesiek/Documents/Codex/hostapd-mlo-address-before-20261004.log`.

After the fix, all 20 tests pass on both prepared source variants with ASan,
UBSan and leak detection. Logs in the same directory:
`hostapd-mlo-address-after-20261004.log` and
`hostapd-mlo-address-wpad-after-20261004.log`.
`make -j2 package/network/services/hostapd/compile V=s` succeeds for
hostapd/full-openssl and wpad/full-mbedtls, producing release-5 APKs.
Build log: `/tmp/flint3-mlo-address-build-20261004.log`.
There are no compiler errors, failed hunks or patch fuzz; line offsets are
expected when later patches change positions. Only the previously seen
jobserver/ninja parallelism warnings occur.

No changes to country, transmit power, channel, encryption, router settings,
kernel, ath12k, firmware, fancontrol or translations are needed.

### Second iteration: full image and router verification

The user built and installed the release-5 image at commit
`8598114d1af2b32041cf38bd962b1023394f96c2`. Its sysupgrade SHA-256 is
`04ba887381c4a4ae5c17564827de13b09ce0814266d8f653b2348c1e78998003`.
The completed full-build log is
`/home/grzesiek/Documents/Codex/flint3-mlo-address-full-build-20261004.log`.
Profile metadata identifies this commit and kernel 6.18.55; the manifest
contains hostapd/common/utils release 5. All eight image-output checksums
verify, and the hostapd binary in the SquashFS matches the newly packaged
binary. The full build has jobserver/libtool warnings and future-file-time
warnings in hwinfo, but no compiler errors or failed build targets.

The supplied router output on 2026-10-04 shows:

```text
20:22:04 phy0.2-ap0: AP-ENABLED
20:22:05 phy0.0-ap0: AP-ENABLED
20:22:06 phy0.1-ap0: AP-ENABLED
ap-mld0 link ID 1: 96:83:c4:ce:a3:f8, channel 36, 5 GHz, 80 MHz
ap-mld0 link ID 2: 00:03:7f:12:82:a2, channel 21, 6 GHz, 320 MHz
```

All three ordinary APs are enabled. Both MLO links are present with different
link addresses, even with the previously failing 6 GHz-first startup order.
The supplied filtered log contains no MLD link-add error or signal-11 exit.
The user then confirms that everything works, including router/internet
access on ordinary 5 GHz and the MLO SSID.

This verifies the startup correction on the router. It is not a multi-day
firmware stability test or a measurement of simultaneous multi-link traffic.
The description-only follow-up commit does not require another firmware build.

## Build and router check

```sh
cd /home/grzesiek/openwrt-flint3
set -o pipefail
make -j"$(nproc)" V=s 2>&1 | tee /home/grzesiek/Documents/Codex/flint3-mlo-address-full-build-20261004.log
```

After installing the resulting image, collect these on the router after
about two minutes:

```sh
uptime
iw dev
logread | grep -E 'signal: 11|hostapd.*(link_add failed|Failed to add link|AP-ENABLED|AP-DISABLED|AP-STA-CONNECTED|EAPOL-4WAY)'
```

The new `MLD: link_add failed` line, if it occurs, is important: it retains
the error number and MAC missing from the original log. A surviving daemon
alone is not sufficient evidence; verify all three radios, MLO link IDs 1/2,
and internet access on ordinary 5 GHz and MLO.
The MLO link IDs 1 and 2 must have different link addresses even when 6 GHz
starts first; sharing the interface's MLD address is not a requirement for
individual link BSSIDs.
