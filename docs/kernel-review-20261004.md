# Flint 3: kernel update assessment, 2026-10-04

This is the historical assessment made against kernel 6.18.52, before the
separate upgrade branch was started. Implementation and verification of the
later upgrade are tracked in
[kernel-6.18.55-flint3-20261004.md](kernel-6.18.55-flint3-20261004.md).

## Version and recommendation

This branch still uses 6.18.52. According to
[kernel.org](https://www.kernel.org/), the latest release in the same longterm
series is **6.18.55**, released 2026-10-03. The current stable series is 7.2.9;
moving to it is a larger porting task, not needed for the fixes below.

Official OpenWrt main currently uses **6.18.54**, introduced by
[142619bac5ba](https://github.com/openwrt/openwrt/commit/142619bac5ba56d5c8adbb80b6a65095057e0a65),
following 6.18.53 in
[411a8112dd12](https://github.com/openwrt/openwrt/commit/411a8112dd1211b47469fd99dc9c0d25560f5b1c).
Both commits also refresh/remove OpenWrt patches. Updating only the version and
tarball hash would miss that maintenance.

Recommendation: keep this ath12k experiment on 6.18.52 for an isolated test,
then update the kernel in another branch. Use the OpenWrt 6.18.53/54 maintenance
as the reviewed foundation and assess/refresh the remaining patch stack for
6.18.55. Staying within 6.18 is preferable to jumping to 7.x for this router.
No kernel upgrade had been implemented or compiled at the assessment stage.

## Changes relevant to this build

| Release | Change | Applicability |
| --- | --- | --- |
| [6.18.53](https://cdn.kernel.org/pub/linux/kernel/v6.x/ChangeLog-6.18.53) | Bridge multicast list converted properly to RCU (`4b772869a1e5`). | Bridge IGMP snooping is enabled. Current 6.18.52 still uses the old list operations. Useful memory-lifetime correction, not a proven cause of a user-observed failure. |
| [6.18.54](https://cdn.kernel.org/pub/linux/kernel/v6.x/ChangeLog-6.18.54) | Flowtable keeps its conntrack reference until the flow is released after an RCU grace period (`e75a9fa1d44b`). | NF_FLOW_TABLE/NFT_FLOW_OFFLOAD are built. Relevant when flows are offloaded; not necessarily exercised by the current dumb-AP bridge traffic. |
| 6.18.54 | NAT hook error cleanup and nftables hook name/prefix matching corrections. | Included networking components benefit in the affected error/control-plane cases. These are not PPE FIFO tuning. |
| [6.18.55](https://cdn.kernel.org/pub/linux/kernel/v6.x/ChangeLog-6.18.55) | Bridge MDB deletion restarts its walk to avoid a stale cursor (`ab1404ac8115`). | Applicable to the bridge multicast code; current source lacks this fix. |
| 6.18.55 | Hardware flow-offload worker publishes HW_DEAD only after it finishes accessing the flow (`d644b23afe1e`). | Fixes a use-after-free race in hardware-offload teardown. The old ordering is present locally; runtime relevance depends on hardware flow-offload use. |
| 6.18.55 | TCP SYN-ACK retransmit hint use-after-free fix, IPv6 header bounds checks and nftables hook-list RCU handling. | General network-stack correctness/hardening. No evidence these explain the current MLO startup messages. |
| 6.18.55 | SquashFS XZ dictionary-size validation (`1f7745fb3580`). | SquashFS/XZ are enabled. Protects handling of malformed images, not a normal boot performance improvement. |

The flowtable, bridge and SquashFS conclusions were checked against the actual
patched `build_dir/.../linux-6.18.52` source and its generated kernel config,
not solely inferred from commit titles.

## Changes not to confuse with new Wi-Fi fixes

6.18.53 backports the ath12k multi-radio channel-context switch correction
[`675aa75bfc29`](https://kernel.googlesource.com/pub/scm/linux/kernel/git/torvalds/linux/+/675aa75bfc29fb18c6e4d58904a91c1d37228217).
Our backports 7.2 `ath12k_mac_op_switch_vif_chanctx()` already validates old/new
radio contexts and groups vifs by their associated radio. This change is
therefore already represented in the driver used by this build.

Wi-Fi comes from `package/kernel/mac80211` backports 7.2, independently of the
base kernel's in-tree Wi-Fi sources. Merely selecting 6.18.55 does not replace
that package with the 6.18.55 ath12k/mac80211 versions or automatically apply
every Wi-Fi item in the stable changelogs.

The 6.18.54 ARM64 LSE per-CPU fixes do not exercise their LSE path here:
`CONFIG_ARM64_USE_LSE_ATOMICS` is disabled. The 6.18.55 QCOM ADSP IOMMU cleanup
does not target our WCSS loader: `CONFIG_QCOM_Q6V5_ADSP` is disabled while
`CONFIG_QCOM_Q6V5_WCSS` is enabled. The checked changelogs do not identify a
Flint-specific PPE RX FIFO fix or an additional CAKE fix.

## Required checks for a later upgrade

- Import the applicable OpenWrt refreshes and remove backports superseded by
  the new stable kernel, preserving Flint-specific changes.
- Apply the complete generic/qualcommbe patch stack without fuzz; review
  conflicts, especially bridge, netfilter, DSA/PPE and remoteproc paths.
- Check configuration differences and build the kernel, out-of-tree modules
  and complete image together. Re-run the host regression suites.
- Keep the known-good image and verify boot, all Wi-Fi bands, DHCP/roaming,
  Ethernet, throughput and recovery on the router before considering it stable.
