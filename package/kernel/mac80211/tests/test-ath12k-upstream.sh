#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# Run actual patched AHB/TX callbacks against offline synthetic state.
set -euo pipefail
ath_test_dir=$(cd -- "$(dirname -- "$0")" && pwd)
ath_test_repo=$(cd -- "$ath_test_dir/../../../.." && pwd)
ath_test_src=${1:-"$ath_test_repo/build_dir/target-aarch64_cortex-a53_musl/linux-qualcommbe_ipq53xx/mac80211-regular/backports-7.2"}
ath_test_tmp=$(mktemp -d /tmp/ath12k-upstream-test.XXXXXX)
ath_test_driver="$ath_test_src/drivers/net/wireless/ath/ath12k"
extract_function() {
	awk -v name="$2" '$0 ~ "^" name { p=1 } p { print } p && /^}/ { exit }' "$1"
}
{
	cat "$ath_test_dir/ath12k-upstream-stubs.h"
	extract_function "$ath_test_driver/ahb.c" 'static int ath12k_ahb_configure_rproc'
	extract_function "$ath_test_driver/hal.c" 'int ath12k_hal_srng_src_num_free'
	extract_function "$ath_test_driver/wifi7/hw.c" 'static u8 ath12k_wifi7_hw_get_ring_selector_qcn9274'
	extract_function "$ath_test_driver/wifi7/hw.c" 'static u8 ath12k_wifi7_hw_get_ring_selector_wcn7850'
	extract_function "$ath_test_driver/wifi7/hw.c" 'static void ath12k_wifi7_mac_op_wake_tx_queue'
	cat "$ath_test_dir/ath12k-upstream-main.c"
} | "${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Werror \
	-Wno-unused-parameter -fsanitize=address,undefined -fno-omit-frame-pointer \
	-x c -o "$ath_test_tmp/test-ath12k-upstream" -
"$ath_test_tmp/test-ath12k-upstream"
printf 'HOST_TEST_DIR=%s\n' "$ath_test_tmp"
