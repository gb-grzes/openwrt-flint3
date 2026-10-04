# Flint 3: failed MLO link setup and hostapd startup crashes

Branch: `fix-mlo-startup-20261004`, based on `update-app-sources-20261004`
at `71d059856e526c031acdf8a9d805348b0f10916a`.

## Evidence and scope

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

## Changes

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

## Validation

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

No new full image has been built or flashed for this change yet. A router
boot test is required, particularly to confirm both MLO links remain present
and that ordinary 5 GHz still works without relying on process crashes to retry.

The currently installed, internet-working application-update image is saved
with its manifest and profile metadata in
`/home/grzesiek/Documents/Codex/flint3-before-mlo-startup-20261004.OgvAJb`.
Its sysupgrade SHA-256 is
`4ed2d9e3001dafd6dd8f82f09522710b58e5025950d066c53558758f19739d80`.

## Build and router check

```sh
cd /home/grzesiek/openwrt-flint3
set -o pipefail
make -j"$(nproc)" V=s 2>&1 | tee /home/grzesiek/Documents/Codex/flint3-mlo-startup-full-build-20261004.log
```

After installing the resulting image, collect these on the router after
about two minutes:

```sh
uptime
iw dev
logread | grep -E 'hostapd.*(signal: 11|link_add failed|Failed to add link|AP-ENABLED|AP-STA-CONNECTED|EAPOL-4WAY)'
```

The new `MLD: link_add failed` line, if it occurs, is important: it retains
the error number and MAC missing from the original log. A surviving daemon
alone is not sufficient evidence; verify all three radios, MLO link IDs 1/2,
and internet access on ordinary 5 GHz and MLO.
