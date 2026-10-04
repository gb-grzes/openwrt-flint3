#!/usr/bin/env python3
"""Run native tests using MLD cleanup functions from the prepared hostapd source.

These tests do not run a radio or alter the router. They exercise the actual
list operations with small BSS/driver stubs, including a failed second link.
"""

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"^[\w *\n]+\b" + re.escape(name) + r"\([^;]*?\)\s*\{",
                      source, re.M)
    if not match:
        raise ValueError(f"Function not found: {name}")
    start = source.index("{", match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


PRELUDE = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "list.h"
#define CONFIG_IEEE80211BE 1
#define MAX_NUM_MLD_LINKS 16
#define ETH_ALEN 6
#define MSG_DEBUG 0
#define MSG_ERROR 1
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define os_free free
#define os_memset memset
typedef uint8_t u8;
struct hostapd_data;
struct hostapd_bss_config { int mld_ap; char iface[32]; void *vlan; };
struct hostapd_iface { struct hostapd_data *bss[2]; void *interfaces; };
struct hostapd_mld {
    struct dl_list links;
    unsigned int num_links;
    struct hostapd_data *fbss;
    char name[32];
    unsigned int refcount;
};
struct driver_ops { int (*link_add)(void *, u8, const u8 *, void *); };
struct hostapd_data {
    struct hostapd_bss_config *conf;
    struct hostapd_iface *iface;
    struct hostapd_mld *mld;
    struct dl_list link;
    struct { void *resp_sta_profile; } partner_links[MAX_NUM_MLD_LINKS];
    void *drv_priv;
    const struct driver_ops *driver;
    int started;
    unsigned int mld_link_id;
};
static char diagnostic[256];
static int driver_result;
static int driver_calls;
static void wpa_printf(int level, const char *format, ...) {
    va_list ap;
    if (level != MSG_ERROR) return;
    va_start(ap, format);
    vsnprintf(diagnostic, sizeof(diagnostic), format, ap);
    va_end(ap);
}
static int fake_link_add(void *priv, u8 id, const u8 *addr, void *ctx) {
    assert(priv && ctx && addr && id == 1);
    driver_calls++;
    return driver_result;
}
int hostapd_mld_remove_link(struct hostapd_data *hapd);
'''

CLEANUP_STUBS = r'''
static int freed_ucode, freed_ubus, cleaned_mlds;
static void hostapd_free_hapd_data(struct hostapd_data *hapd) {
    hapd->started = 0;
}
static void hostapd_ucode_free_bss(struct hostapd_data *hapd) {
    assert(hapd);
    freed_ucode++;
}
static void hostapd_ubus_free_bss(struct hostapd_data *hapd) {
    assert(hapd);
    freed_ubus++;
}
static void hostapd_cleanup_unused_mlds(void *interfaces) {
    (void)interfaces;
    cleaned_mlds++;
}
'''

CASES = r'''
static struct hostapd_mld mld;
static struct hostapd_bss_config cfg[3];
static struct hostapd_iface iface;
static struct hostapd_data bss[3];
static const struct driver_ops driver = { fake_link_add };
static void init(void) {
    memset(&mld, 0, sizeof(mld));
    memset(cfg, 0, sizeof(cfg));
    memset(&iface, 0, sizeof(iface));
    memset(bss, 0, sizeof(bss));
    strcpy(mld.name, "ap-mld0");
    dl_list_init(&mld.links);
    iface.bss[0] = &bss[0];
    iface.bss[1] = &bss[1];
    for (unsigned int i = 0; i < 3; i++) {
        cfg[i].mld_ap = 1;
        strcpy(cfg[i].iface, "ap-mld0");
        bss[i].conf = &cfg[i];
        bss[i].iface = &iface;
        bss[i].mld = &mld;
        bss[i].mld_link_id = i;
        bss[i].drv_priv = &iface;
        bss[i].driver = &driver;
    }
}
static void check_count(unsigned int n) {
    assert(mld.num_links == n);
    assert(dl_list_len(&mld.links) == n);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    init();
    const char *test = argv[1];
    if (!strcmp(test, "failed-second-link")) {
        hostapd_mld_add_link(&bss[0]);
        bss[1].started = 1; /* setup_bss sets this before driver link_add */
        bss[1].partner_links[0].resp_sta_profile = malloc(32);
        hostapd_bss_link_deinit(&bss[1]);
        check_count(1);
        assert(mld.fbss == &bss[0]);
        assert(!bss[1].partner_links[0].resp_sta_profile);
    } else if (!strcmp(test, "registered-first-driver-cleared")) {
        hostapd_mld_add_link(&bss[0]);
        bss[0].drv_priv = NULL;
        hostapd_bss_link_deinit(&bss[0]);
        check_count(0);
        assert(!mld.fbss);
    } else if (!strcmp(test, "registered-secondary-before-start")) {
        hostapd_mld_add_link(&bss[0]);
        hostapd_mld_add_link(&bss[1]);
        hostapd_bss_link_deinit(&bss[1]);
        check_count(1);
        assert(mld.fbss == &bss[0]);
    } else if (!strcmp(test, "remove-never-added")) {
        hostapd_mld_add_link(&bss[0]);
        assert(hostapd_mld_remove_link(&bss[1]) == 0);
        check_count(1);
    } else if (!strcmp(test, "remove-twice")) {
        hostapd_mld_add_link(&bss[0]);
        assert(hostapd_mld_remove_link(&bss[0]) == 0);
        assert(hostapd_mld_remove_link(&bss[0]) == 0);
        check_count(0);
    } else if (!strcmp(test, "first-link-promotion")) {
        for (unsigned int i = 0; i < 3; i++) hostapd_mld_add_link(&bss[i]);
        cfg[0].vlan = &iface;
        bss[0].started = 1;
        hostapd_bss_link_deinit(&bss[0]);
        check_count(2);
        assert(mld.fbss == &bss[1]);
        assert(cfg[1].vlan == &iface && !cfg[0].vlan);
        bss[1].started = 1;
        hostapd_bss_link_deinit(&bss[1]);
        check_count(1);
        assert(mld.fbss == &bss[2]);
        bss[2].started = 1;
        hostapd_bss_link_deinit(&bss[2]);
        check_count(0);
        assert(!mld.fbss);
    } else if (!strcmp(test, "deinit-twice")) {
        hostapd_mld_add_link(&bss[0]);
        bss[0].started = 1;
        hostapd_bss_link_deinit(&bss[0]);
        hostapd_bss_link_deinit(&bss[0]);
        check_count(0);
    } else if (!strcmp(test, "plain-bss")) {
        cfg[1].mld_ap = 0;
        hostapd_mld_add_link(&bss[0]);
        assert(hostapd_mld_remove_link(&bss[1]) == 0);
        hostapd_bss_link_deinit(&bss[1]);
        check_count(1);
    } else if (!strcmp(test, "missing-mld-context")) {
        bss[1].mld = NULL;
        assert(hostapd_mld_remove_link(&bss[1]) == -1);
        hostapd_bss_link_deinit(&bss[1]);
        check_count(0);
    } else if (!strcmp(test, "driver-error-diagnostic")) {
        const u8 addr[6] = {0x96, 0x83, 0xc4, 0xce, 0xa3, 0xf8};
        driver_result = -EALREADY;
        assert(hostapd_drv_link_add(&bss[1], 1, addr) == -EALREADY);
        assert(driver_calls == 1);
        assert(strstr(diagnostic, "link_id=1"));
        assert(strstr(diagnostic, "96:83:c4:ce:a3:f8"));
        assert(strstr(diagnostic, "ret=-114"));
    } else if (!strcmp(test, "driver-success-quiet")) {
        const u8 addr[6] = {0x96, 0x83, 0xc4, 0xce, 0xa3, 0xf8};
        assert(hostapd_drv_link_add(&bss[1], 1, addr) == 0);
        assert(driver_calls == 1 && !diagnostic[0]);
    } else if (!strcmp(test, "missing-driver")) {
        const u8 addr[6] = {0};
        bss[1].driver = NULL;
        assert(hostapd_drv_link_add(&bss[1], 1, addr) == -1);
        assert(driver_calls == 0);
    } else if (!strcmp(test, "dynamic-failure-after-link-add") ||
               !strcmp(test, "dynamic-failure-before-link-add")) {
        struct hostapd_data *failed = malloc(sizeof(*failed));
        assert(failed);
        *failed = bss[1];
        mld.refcount = 2; /* setup_multi_link() has referenced both BSSes */
        hostapd_mld_add_link(&bss[0]);
        if (!strcmp(test, "dynamic-failure-after-link-add"))
            hostapd_mld_add_link(failed);
        failed->started = 1;
        dynamic_failure_cleanup(failed, &iface);
        check_count(1);
        assert(mld.fbss == &bss[0] && mld.refcount == 1);
        assert(freed_ucode == 1 && freed_ubus == 1 && cleaned_mlds == 1);
    } else {
        return 2;
    }
    puts(test);
    return 0;
}
'''

TESTS = [
    "failed-second-link", "registered-first-driver-cleared",
    "registered-secondary-before-start", "remove-never-added", "remove-twice",
    "first-link-promotion", "deinit-twice", "plain-bss", "missing-mld-context",
    "driver-error-diagnostic", "driver-success-quiet", "missing-driver",
    "dynamic-failure-after-link-add", "dynamic-failure-before-link-add",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_dir", type=Path, help="prepared hostapd source")
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--test", choices=TESTS, action="append")
    args = parser.parse_args()
    source = (args.source_dir / "src/ap/hostapd.c").read_text()
    driver_source = (args.source_dir / "src/ap/ap_drv_ops.h").read_text()
    funcs = [function(source, name) for name in (
        "hostapd_mld_move_vlan_list", "hostapd_mld_add_link",
        "hostapd_mld_remove_link", "hostapd_bss_link_deinit")]
    funcs.append(function(driver_source, "hostapd_drv_link_add"))
    ucode_source = (args.source_dir / "src/ap/ucode.c").read_text()
    dynamic_add = function(ucode_source, "uc_hostapd_iface_add_bss")
    cleanup = dynamic_add.split("\nfree_hapd:\n", 1)[1].split("\nout:\n", 1)[0]
    funcs.append(CLEANUP_STUBS +
                 "\nstatic void dynamic_failure_cleanup(struct hostapd_data *hapd, "
                 "struct hostapd_iface *iface) {\n(void)iface;\n" + cleanup + "\n}\n")
    with tempfile.TemporaryDirectory(prefix="hostapd-mlo-test-") as directory:
        temp = Path(directory)
        harness = temp / "mlo.c"
        harness.write_text(PRELUDE + "\n".join(funcs) + CASES)
        binary = temp / "mlo-test"
        subprocess.run([args.cc, "-std=c99", "-Wall", "-Wextra", "-Werror",
                        "-g", "-O1", "-fsanitize=address,undefined", "-fno-pie",
                        "-no-pie", "-fno-omit-frame-pointer", "-I",
                        str(args.source_dir / "src/utils"), str(harness),
                        "-o", str(binary)], check=True)
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1")
        results = []
        for test in args.test or TESTS:
            result = subprocess.run([str(binary), test], env=env,
                                    capture_output=True, text=True)
            print(f"{'PASS' if result.returncode == 0 else 'FAIL'} {test}")
            if result.returncode:
                print(result.stderr)
            results.append(result.returncode == 0)
        print(f"{sum(results)}/{len(results)} passed (ASan + UBSan)")
        return 0 if all(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
