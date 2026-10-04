# Flint 3: selected ath12k updates and firmware compatibility review

## Scope and result

Branch: `integrate-ath12k-upstream-20261004`, based on tested commit
`4bda6a844a3c2e0738e30710d2afc8826174684a`.

Two of the three requested changes are implemented. Shared RO/MultiPD loading
is deferred after the firmware compatibility review below. Kernel 6.18.52,
backports 7.2, firmware, DTS and user configuration are unchanged. The mac80211
package release increases from 2 to 3.

The existing LED, 802.11k/CAKE, RTL837x Wi-Fi roaming/FDB and iwinfo TX-power
reporting fixes remain. MLO startup diagnostics are deliberately out of scope;
this branch does not claim to fix the hostapd `Failed to add link` messages.
No router changes, firmware flashing or GitHub push were performed.

## Implemented changes

### Ring-capacity-aware TX queue handling

`148-wifi-ath12k-pass-access-category-to-ring-selector.patch` ports only the
ath12k part of the prerequisite
[v7 2/4](https://lore.kernel.org/all/20260917064825.12747-3-jtornosm@redhat.com/).
The selector accepts an access category instead of an skb. QCN9274/IPQ5332
still select by CPU; WCN7850/QCC2072 still select by access category. No ath11k
driver changes are needed.

`149-wifi-ath12k-check-ring-capacity-before-TX-dequeue.patch` adapts
[v7 4/4](https://lore.kernel.org/all/20260917064825.12747-5-jtornosm@redhat.com/).
The new queue-wake callback checks the selected TCL ring before dequeuing,
refreshes its hardware consumer pointer and serializes wake operations with
a per-ring bottom-half-safe lock. The existing frame TX callback remains
unchanged. Additional backport guards reject an invalid link index, absent
link/radio/datapath and crash-flush state before accessing ring memory.

This is an experimental backport of the published patch series, not a claim
of upstream acceptance or a proven fix for every Q6 firmware hang. The author
reports testing on WCN7850, not Flint 3/QCN9274. The precheck uses the default
or association link; per-frame link selection, ring retries and multicast
replication still follow the existing driver. It does not reserve capacity
across all MLO links. Real-device load and recovery testing remain necessary.

### AHB initialization failure cleanup

`338-wifi-ath12k-release-rproc-on-IRQ-setup-failure.patch` adapts
[Wentao Liang's IRQ error-unwind patch](https://lore.kernel.org/all/20260917120443.2150370-1-vulab@iscas.ac.cn/).
If IRQ configuration fails, initialization now unregisters the notifier and
drops the rproc reference through the existing cleanup labels. Successful boot
and the existing firmware authentication/recovery sequence are unchanged.
This is failure-path cleanup, not a fix for a normally running firmware crash.

## Shared RO/MultiPD: reviewed, not enabled

The requested
[shared-RO change c3bace8584ca](https://kernel.googlesource.com/pub/scm/linux/kernel/git/ath/ath/+/c3bace8584ca707e34ba837f65c2e9d566af4c2c)
loads a separate `q6_fw4.mbn` and authenticates RO protection domain 4 before
loading the individual UserPD images. It was tested upstream on IPQ5332 with
WBE 1.6-01275. A matching version string alone is not compatibility evidence.

The current board uses split ELF/MDT firmware: `q6_fw0.mdt`, `q6_fw1.mdt`,
`iu_fw.mdt` and their `.bXX` segments. The DTS explicitly names these files.
The AHB firmware reports WBE 1.6-01270; PCI QCN9274 reports WBE 1.6-01243.
There is no `q6_fw4` in the installed IPQ5332 package.

The current loader also predates the upstream shared RootPD state and AHB
descriptor prerequisites. It caches a mapping per device and uses a two-bit
UserPD ID mask; RO domain 4 requires the wider mask and shared ownership.
The port must preserve patches 331/335 (independent UserPD reload and completion
ordering), fixed reserved-memory addresses, HOSTDDR at 0x4b500000, secure PAS
authentication and the existing RootPD shutdown contract. Adding one filename
or renaming an image is insufficient.

Public-source review on 2026-10-04:

| Source | Finding | Decision |
| --- | --- | --- |
| [Current Qualcomm/CodeLinaro repository](https://git.codelinaro.org/clo/ath-firmware/ath12k-firmware) at `772f708f38c58d87e07538db05188864cfecadfd` | IPQ5322 contains board data only. IPQ5424 has 1.6-01275 with q6_fw0, q6_fw1, iu_fw and qdsp6sw_dtb; no q6_fw4. | Not a verified IPQ5332 RO bundle. Do not substitute IPQ5424 firmware. |
| [Archived Qualcomm GitHub repository](https://github.com/quic/upstream-wifi-fw) | q6_fw4.mdt exists in old 1.3/1.3.1 packages for IPQ5322 with two QCN6432 radios. The archive directs users to CodeLinaro. | Different radio topology and firmware generation; no documented Flint 3 compatibility. |
| [linux-firmware ath12k](https://gitlab.com/kernel-firmware/linux-firmware/-/tree/main/ath12k) | Contains IPQ5424, QCC2072, QCN9274 and WCN7850, but no IPQ5332 RO bundle. | No suitable replacement established. |

The working QCN9274 `firmware-2.bin` from linux-firmware-20260810 has Git blob
ID `ea9c18e8aba3fa74bf968c586eac8c72631511f8`, identical to the current
linux-firmware main tree's QCN9274 blob. The current CodeLinaro QCN9274 1.6
directory also tops out at 01243. Updating the general firmware package is
therefore not an established QCN9274 executable-firmware upgrade.

No new binary was added. A future MultiPD implementation needs a vendor-provided
or otherwise documented matching IPQ5332 signed RO/UserPD bundle, verification
of its memory/PAS contract, redistribution notice preservation, and controlled
2.4 GHz boot/recovery tests with a known-good image available. Availability in
the checked public sources is not evidence that no such vendor bundle exists.

## Verification completed locally

- All 235 mac80211 patches applied to a fresh, hash-verified backports 7.2
  archive with `patch --fuzz=0`. Existing patches may have line offsets; no
  context fuzz was permitted. All seven source files touched by the new patches
  match the prepared and cross-compiled source tree byte for byte.
- 16 new regression groups passed with AddressSanitizer and
  UndefinedBehaviorSanitizer. They compile the actual patched AHB initializer,
  TX callback, ring selectors and ring free-space calculation against synthetic
  state. Cases cover resource unwind, empty/full/wrapping rings, lock balancing,
  association/default link IDs other than zero, absent objects and crash-flush.
- The later-wake test explicitly simulates hardware progress and another wake
  call. It does not prove real hardware completion always reschedules a queue.
- The existing 13 RTL837x FDB and 16 iwinfo TX-power groups passed again.
- `make package/kernel/mac80211/compile -j4 V=s` exited 0, producing AArch64
  ath12k and ath12k_wifi7 modules for kernel 6.18.52. This was a package build,
  not a full firmware image build or router test.
- Build output includes jobserver, MODULE_DESCRIPTION, QCOM_MDT_LOADER/ARC4
  Kconfig and empty optional-package warnings. These configurations/metadata
  were not changed by the new patches. The build is not warning-free, but
  has no compilation/link failure.
- `.config` SHA-256 remains
  `a0f5d31055c39c930129f53eba0579f69094ecfdfb58c5466220d9716ea4f999`.

Local build logs:
`/home/grzesiek/Documents/Codex/ath12k-upstream-prepare-20261004.log` and
`/home/grzesiek/Documents/Codex/ath12k-upstream-compile-20261004.log`.

Reproduce host tests after preparing the package:

```sh
cd /home/grzesiek/openwrt-flint3
make package/kernel/mac80211/prepare V=s
bash package/kernel/mac80211/tests/test-ath12k-upstream.sh
```

If proceeding to a full image test, build this branch with the existing
configuration. Keep the tested image from the previous branch for rollback.
Verify all three ordinary SSIDs, MLO, DHCP, roaming between TP-Link and Flint,
and sustained traffic with several clients. Examine logs for TX -12 errors,
Q6 recovery and stalled traffic. Do not independently install the rebuilt
modules on a router with a different image/kernel ABI.

Kernel update findings are recorded separately in
[kernel-review-20261004.md](kernel-review-20261004.md).
