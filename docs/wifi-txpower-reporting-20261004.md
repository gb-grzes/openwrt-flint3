# Flint 3: correct Wi-Fi TX-power reporting

## Scope

Read-only reporting fix in `libiwinfo`, on top of the tested
`fix-rtl837x-wifi-fdb-20261004` branch. The kernel remains 6.18.52. The library
ABI remains 20230701 and the package release increases from 1 to 2.

This change does not set TX power, change the regulatory country, edit wireless
configuration, restart Wi-Fi, or modify ath12k firmware. The earlier LED,
802.11k/CAKE and Wi-Fi FDB fixes remain in the branch.

## Observed problem and cause

On the router, `iw dev phy0.1-ap0 info` reported 22.00 dBm, while
`ubus call iwinfo txpowerlist '{"device":"radio1"}'` returned a single entry:
255 dBm, 2147483647 mW, inactive. LuCI displayed that entry and a current power
of 0 dBm. The user reports the same display problem on all three radios.

The original library reused one integer as both channel input and maximum-power
output. If no limit was found, -1 was copied into the unsigned 8-bit dBm field,
becoming 255. Converting that invalid value to mW overflowed the integer return.
The old getter also treated a successful netlink reply with a missing TX-power
attribute as a successful reading of zero.

Additionally, resolving a UCI radio by its shared wiphy selected the first
netdev, which could be the MLD or a different band. A channel number alone is
not unique across 2.4 and 6 GHz. Non-split wiphy responses omit higher bands,
and split responses can contain fragments without frequencies.

There is no established evidence that upgrading from kernel 6.18.44 caused the
issue. The reporting code, kernel netlink layout and interface selection all
matter; the running 6.18.44 image was not available for a side-by-side test.

## Implementation

- Query configured UCI radios using a wiphy-filtered interface dump. Match the
  configured band and, when present, the physical radio mask. No reliance on
  interface enumeration order or on MLO link IDs equalling radio indices.
- Read the current operating frequency and TX power from ordinary interfaces
  or nested MLO links. Band-specific radio queries work in an MLO-only setup.
- For a whole MLD, expose one current-power value only if all reported links
  agree and supply a power value. Otherwise leave it unknown; do not borrow
  the power from a different link. A multi-frequency MLD has no single
  frequency for a power-option list.
- Request split wiphy replies and match the exact operating frequency. Preserve
  the result across capabilities-only and unrelated-band fragments. Ignore
  disabled channels and retain the lowest limit if a frequency is repeated.
- Return an empty list on unavailable or unrepresentable limits, never a
  wrapped sentinel. Preserve the existing unsigned dBm/16-bit mW ABI: 0 through
  48 dBm are representable whole-dBm options. Legitimate zero is not mistaken
  for missing data.
- Keep LuCI and rpcd unchanged: their existing error handling omits unavailable
  readings, and LuCI already displays unknown when current power is absent.

The split-dump issue is also described in
[OpenWrt iwinfo PR #41](https://github.com/openwrt/iwinfo/pull/41). This local
patch additionally handles the shared-wiphy and MLO power queries. It is not a
claim that the upstream proposal has been merged.

The separate ucode `/usr/bin/iwinfo` frontend is not changed here. Use the
`ubus` calls below when verifying the data used by LuCI.

## Offline verification

Prepare the source, then run the actual patched getters and callbacks against
synthetic netlink/UCI replies:

```sh
make package/network/utils/iwinfo/prepare V=s
ASAN_OPTIONS=detect_leaks=0 bash package/network/utils/iwinfo/tests/test-txpower.sh
```

The host tests use the package's real nl80211 definitions, real iwinfo ABI and
real dBm-to-mW conversion, with AddressSanitizer and UndefinedBehaviorSanitizer.
Leak checking is disabled in the command because the sandbox uses ptrace;
the host netlink fixtures themselves are statically allocated.

The suite covers all three radios on one wiphy, MLD-first ordering, ordinary
interfaces, absent and genuine-zero power, signed mBm, equal and differing
MLO power, MLO-only band selection, missing link values, overlapping channel
numbers, split fragments, absent/disabled/overflowing limits, the 0/48 dBm
boundaries, duplicate limits, invalid radio indices, wrong masks, ambiguous
radio selection and error/conveyor-ownership paths.

The AArch64 package cross-build is:

```sh
make package/network/utils/iwinfo/compile V=s
```

Build and test records are local in
`/home/grzesiek/Documents/Codex/flint3-dhcp-diag-20261003/`:

- `iwinfo-txpower-prepare-20261004.log`
- `iwinfo-txpower-build-20261004-verified.log`
- `iwinfo-txpower-tests-20261004.log`

All 16 TX-power regression groups passed with sanitizers. The final AArch64
library/package build exited with status 0 and no compiler warnings/errors.
The existing 13 RTL837x FDB regression groups also passed again.

The build configuration is unchanged (SHA-256
`a0f5d31055c39c930129f53eba0579f69094ecfdfb58c5466220d9716ea4f999`).

## Router verification after the user builds and installs a new image

Build a full image from this branch on Aspire:

```sh
cd /home/grzesiek/openwrt-flint3
git switch fix-wifi-txpower-reporting-20261004
make -j"$(nproc)" V=s
```

After installing it, run these read-only checks **on the router**:

```sh
iw dev
for radio in radio0 radio1 radio2; do
    ubus call iwinfo txpowerlist "{\"device\":\"$radio\"}"
done
ubus call iwinfo info '{"device":"phy0.0-ap0"}'
ubus call iwinfo info '{"device":"phy0.1-ap0"}'
ubus call iwinfo info '{"device":"phy0.2-ap0"}'
ubus call iwinfo info '{"device":"ap-mld0"}'
```

Use the interface names from `iw dev` if they differ. Compare current readings
with `iw dev <interface> info` and refresh LuCI. No 255 dBm/overflowed mW entry
should appear. The current power should agree with the driver reading, or be
unknown where a single truthful value is unavailable. The maximum in the
option list is a channel limit, not necessarily the currently used power.

This is driver-reported power, not an RF measurement. Hardware verification
remains pending; this task does not install anything on the router or push
the new branch to GitHub.
