#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# Compile the actual production FDB functions against a host-only SDK model.
# No router access, register writes, module loading or network changes.
set -euo pipefail
rtl_test_dir=$(cd -- "$(dirname -- "$0")" && pwd)
rtl_test_src="$rtl_test_dir/../src/rtl837x_dsa_ops.c"
rtl_test_tmp=$(mktemp -d /tmp/rtl837x-fdb-test.XXXXXX)
grep -q 'ds->assisted_learning_on_cpu_port = gsw->chip_id == CHIP_RTL8372N;' "$rtl_test_src"
{
    cat "$rtl_test_dir/fdb-stubs.h"
    awk '/^static int rtl837x_to_errno/ {p=1}
         /^static int rtl837x_devlink_info_get/ {exit} p' "$rtl_test_src"
    awk '/^static bool rtl837x_valid_port/ {p=1}
         /^static u32 rtl837x_user_ports/ {exit} p' "$rtl_test_src"
    awk '/^static int rtl837x_fdb_vid/ {p=1}
         /^static int rtl837x_port_vlan_fast_age/ {exit} p' "$rtl_test_src"
    awk '/^static int rtl837x_cpu_fdb_add/ {p=1}
         /^static int rtl837x_port_fdb_dump/ {exit} p' "$rtl_test_src"
    cat "$rtl_test_dir/fdb-main.c"
} | "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -x c -o "$rtl_test_tmp/test-fdb" -
"$rtl_test_tmp/test-fdb"
printf 'HOST_TEST_DIR=%s\n' "$rtl_test_tmp"
