# Flint 3 integration — 2026-09-30

This is a separate integration branch, not a tested firmware release.

- Branch: `integrate-perceival-20260930`.
- Stable starting point: `6bed45373e2d6de61ebae567eb6c5f3d1c99b331`
  (`qualcommbe: flint3: poprawa LED przy rozruchu`).
- Merged source: [perceival/flint3-be9300 at 5e05500f27](https://github.com/perceival/openwrt-flint3/commit/5e05500f27473bed8589c99cd58924f0450ecf60).
- The stable `flint3-be9300` branch is not changed by this integration.

## Scope and preserved local changes

The merge brings in the complete source history, including the ath12k/Q6
memory and recovery changes, the RTL837x driver updates and native PPE offload
series through 0454, experimental hostapd FT-over-MLO patches, boot scripts,
optional vendor driver packages, documentation and upstream test tools.

The kernel stays at **6.18.52**, not perceival's 6.18.39. Hostapd stays at
**2026-08-07 / 831364bf**, and mac80211 backports stay at **7.2**. Local
kernel compatibility changes, the 802.11k Link Measurement fix, CAKE fix,
white LED during boot and blue LED when running are preserved. The Wi-Fi
setup guard still disables 802.11r on MLO SSIDs.

No router was accessed or flashed, and the existing build `.config` was not
changed. No Wi-Fi country was changed; the user's intentional US setting is
not overridden by this integration. Existing build outputs are not new
images for this branch.

## Behaviour changes and cautions

- RTL837x now defaults to the native `rtl8_4` tagger. The corresponding PPE
  parser changes apply even when hardware flow offload is disabled.
- The code does not set the firewall's `flow_offloading_hw` option. However,
  **an existing saved value of `1` can activate the new hardware offload after
  upgrading**. Check it on the router before flashing. The documented shared
  ingress-interface limitation can weaken isolation between VLANs/zones with
  hardware offload enabled; keep it disabled for initial testing.
- FT-over-MLO source code is included, but the local 11r/MLO guard is retained.
  Its presence does not mean roaming has been validated.
- Low-ACK disconnection defaults to off. Explicit per-SSID configuration can
  still override the default.
- `wsdd2` is stopped and disabled on GL-BE9300 at the first boot after an
  upgrade, including an upgrade that preserves settings. Windows automatic
  share discovery may disappear; direct access by name/address still works.
  This removes a reported trigger, not the underlying netlink panic bug.
- Wi-Fi firmware coredumps are released to allow recovery, with compressed
  copies retained locally when space permits. Dumps can contain Wi-Fi keys;
  do not upload them publicly. The cause of firmware crashes is not fixed.
- The Q6 memory layout deliberately contains the vendor host-DDR sub-window
  inside the Q6 reservation; an overlap warning for those two regions is
  expected. This is separate from the pre-existing PCI bus-number warning.
- The optional `qca-ssdk` / `qca-nss-ppe` packages are imported but are not
  selected in the user's existing build configuration. Their sources were
  not locally available for build validation.

## Integration checks (not a firmware build)

- All 601 kernel patches applied to clean Linux 6.18.52 with `--fuzz=0`.
  Context in PPE patches 0419/0436 was refreshed to retain the existing
  netdevice teardown fix; 2010b was refreshed for the newer MDT source.
- All 72 hostapd patches applied to the hash-verified 2026-08-07 source with
  `--fuzz=0`. Patch 991's context was refreshed; 992/998 were adapted to keep
  FT Authentication responses addressed to the peer link, consistent with
  the newer hostapd Authentication fixes retained in this fork.
- All 232 mac80211 patches applied to hash-verified backports 7.2 with
  `--fuzz=0`. The newer peer-association status handling is retained and
  patch contexts were refreshed. The existing 6.18 crypto-shash compatibility
  patches and BH workqueue implementation are preserved instead of restoring
  obsolete tasklet reverts. The valid-TX-link fix occurs once, as patch 147.
- The merged board device tree compiled. The same pre-existing PCI bus-number
  warning remains. Q6 memory addresses and LED aliases were checked.
- The standard OpenWrt LED state handler passed a mock test using the compiled
  device tree: white boot/failsafe/upgrade, blue running.
- Changed shell scripts passed syntax checking and the Wi-Fi JSON schema
  parsed successfully. The local build `.config` is unchanged.

Patch application and syntax checks do not prove compilation, bootability,
Wi-Fi stability, throughput or VLAN isolation. Build and test this branch
separately, keep the known-good image and have a recovery path before flashing.
