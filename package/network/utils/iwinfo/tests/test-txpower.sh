#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# Run production callbacks/getters against synthetic netlink replies, offline.
# First prepare the package: make package/network/utils/iwinfo/prepare V=s
set -euo pipefail
iw_test_dir=$(cd -- "$(dirname -- "$0")" && pwd)
iw_test_repo=$(cd -- "$iw_test_dir/../../../../.." && pwd)
iw_test_src=${1:-"$iw_test_repo/build_dir/target-aarch64_cortex-a53_musl/libiwinfo-2026.05.26~66bdd1a0"}
iw_test_tmp=$(mktemp -d /tmp/iwinfo-txpower-test.XXXXXX)
{
    cat "$iw_test_dir/txpower-stubs.h"
    awk '/^#define LOG10_MAGIC/ {print}' "$iw_test_src/include/iwinfo/utils.h"
    awk '/^static uint8_t nl80211_freq2band/ {p=1}
         /^static int nl80211_phyname_cb/ {p=0} p' "$iw_test_src/iwinfo_nl80211.c"
    awk '/^struct nl80211_power_data/ {p=1}
         /^static int nl80211_fill_signal_cb/ {p=0} p' "$iw_test_src/iwinfo_nl80211.c"
    awk '/^struct nl80211_txpwr_data/ {p=1}
         /^static void nl80211_get_scancrypto/ {p=0} p' "$iw_test_src/iwinfo_nl80211.c"
    # Keep conversion behavior identical to the actual iwinfo library.
    awk '/^int iwinfo_dbm2mw/ {p=1}
         /^int iwinfo_mw2dbm/ {p=0} p' "$iw_test_src/iwinfo_utils.c"
    cat "$iw_test_dir/txpower-main.c"
} | "${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Werror \
    -Wno-sign-compare -fsanitize=undefined,address -fno-omit-frame-pointer \
    -I"$iw_test_src/include" -I"$iw_test_src" \
    -x c -o "$iw_test_tmp/test-txpower" -
"$iw_test_tmp/test-txpower"
printf 'HOST_TEST_DIR=%s\n' "$iw_test_tmp"
