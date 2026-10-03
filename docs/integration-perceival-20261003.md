# Flint 3 integration — 2026-10-03

This is a new integration branch, not a newly built firmware release.

- Branch: `integrate-perceival-20261003`.
- Starting point: `fb01885e57da73365ef466c2a93ffc29fca9f5bb`, the previous
  `integrate-perceival-20260930` integration.
- Merged source: [perceival/flint3-be9300 at 2365932733](https://github.com/perceival/openwrt-flint3/commit/2365932733ca8ec3b346621d9cec2eb3df3b2cf3).
- Neither the stable `flint3-be9300` branch nor the previous integration
  branch is changed by this update.

## New upstream changes

- `61296a3881`: RTL837x mirror cleanup recovery. Clearing hardware source
  masks is attempted even if disabling mirroring fails. Deletion failures
  also clear the driver's mirror bookkeeping so a subsequent rule can
  reprogram the block; setup clears possibly stale bootloader mirror state.
  These operations are best-effort and still log hardware failures.
- `7fe961bfb6`: ksmbd synchronization with Linux 6.18.52 and OpenWrt main;
  adapted as described below, rather than applying duplicate stable fixes.
- `eb892fd7fd`: documentation reports that the multi-BSS DFS startup/CAC
  loop no longer reproduces upstream. This is not a new Wi-Fi code change
  or a local multi-BSS test result.
- `016195f3de`: PPE hash-bucket exhaustion returns `-ENOSPC` and reports
  table occupancy once per full-table episode, instead of repeated generic
  I/O failure messages. Flows that cannot enter hardware stay in software.
- `058725aa11`: PPE flow re-arm messages use table indices instead of raw
  kernel-address cookies. During a full-table episode they use debug level;
  other re-arms retain their rate-limited informational message.

The complete upstream history is merged. The only omitted upstream source
file is the redundant stable ksmbd backport described next.

## Linux 6.18.52 compatibility and preserved changes

Perceival's kernel baseline is 6.18.39. Its new
`499-ksmbd-server-sync-with-6.18.52-stable.patch` brings that older kernel's
SMB source to 6.18.52. This fork already uses **6.18.52**, so patch 499 is
excluded from the merged tree: it would reapply fixes already in the kernel.
A reverse dry-run against pristine 6.18.52 confirmed that all its hunks are
already present. The six other ksmbd patch files touched by the upstream
commit were already byte-identical in the starting branch and are retained.

The kernel version/hash, hostapd, mac80211, board device tree and Wi-Fi
scripts are unchanged from the previous integration. This preserves the
802.11k Link Measurement and CAKE fixes, white LED during boot, blue LED
when running, and the guard disabling 802.11r on MLO SSIDs. The existing
build `.config` is unchanged, and no Wi-Fi country setting is overridden.

This update does not enable hardware flow offloading or change its existing
VLAN/isolation limitations. The [previous integration notes](integration-perceival-20260930.md)
still apply, including the firmware-recovery and experimental FT-over-MLO
cautions.

## Checks performed

- All **603** kernel patches applied to pristine Linux 6.18.52 with
  `--fuzz=0`, including the two new PPE patches.
- The duplicate ksmbd backport passed a reverse dry-run with `--fuzz=0`
  against pristine Linux 6.18.52.
- The changed RTL837x and PPE code was reviewed, including mirror cleanup
  paths and mutex protection of the full-table episode state.
- The preserved files and build configuration were compared with the
  starting branch; the staged change set passed `git diff --check`.

No full firmware compilation, router access or flashing was performed for
this update. Existing images are from the previous branch and do not contain
these new fixes. Patch application and source review do not establish
bootability, sustained-load stability or hardware behaviour. Build and test
this branch separately, keeping the known-good firmware and a recovery path.
