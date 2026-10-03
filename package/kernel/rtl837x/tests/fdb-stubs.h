/* SPDX-License-Identifier: GPL-2.0 */
/* Host-only SDK/DSA model for the production FDB callbacks. */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint16_t u16;
#define ETH_ALEN 6
#define BIT(n) (1U << (n))
#define RTK_MAX_NUM_OF_PORT 16
#define CHIP_RTL8372N 2
#define CHIP_RTL8373 3
#define DSA_TAG_PROTO_RTL8_4 1
#define DSA_TAG_PROTO_VSC73XX_8021Q 2
enum {
	RT_ERR_OK, RT_ERR_INPUT, RT_ERR_PORT_ID, RT_ERR_PORT_MASK,
	RT_ERR_NULL_POINTER, RT_ERR_MAC, RT_ERR_OUT_OF_RANGE, RT_ERR_ENABLE,
	RT_ERR_RANGE, RT_ERR_VLAN_VID, RT_ERR_L2_FID, RT_ERR_L2_VID,
	RT_ERR_BUSYWAIT_TIMEOUT, RT_ERR_CHIP_NOT_SUPPORTED,
	RT_ERR_DRIVER_NOT_FOUND, RT_ERR_L2_NO_EMPTY_ENTRY,
	RT_ERR_L2_INDEXTBL_FULL, RT_ERR_L2_ENTRY_NOTFOUND,
};
enum { DSA_DB_PORT, DSA_DB_BRIDGE, DSA_DB_LAG };
struct dsa_switch { void *priv; };
struct dsa_port { int index; };
struct dsa_bridge { void *dev; unsigned int num; };
struct dsa_db {
	int type;
	union { struct dsa_port *dp; struct dsa_bridge bridge; };
};
struct rtk_gsw {
	struct dsa_switch ds;
	int chip_id, cpu_port, tag_proto, isolation_lock;
	unsigned int valid_port_mask;
	void *bridge_dev[RTK_MAX_NUM_OF_PORT];
};
typedef struct { unsigned char octet[ETH_ALEN]; } rtk_mac_t;
typedef struct {
	rtk_mac_t mac;
	u16 vid_fid;
	int ivl, port, auth, is_static;
} rtk_l2_ucastAddr_t;

/* Distinct sentinel VIDs verify selection of DSA's existing helpers. */
static u16 dsa_tag_8021q_standalone_vid(const struct dsa_port *dp)
{
	return 100 + dp->index;
}
static u16 dsa_tag_8021q_bridge_vid(unsigned int num) { return 200 + num; }
static void mutex_lock(int *lock) { assert(!*lock); *lock = 1; }
static void mutex_unlock(int *lock) { assert(*lock); *lock = 0; }

static struct {
	bool present;
	rtk_l2_ucastAddr_t entry;
} lut[4];
static unsigned int gets, adds, dels;
static int get_error, del_error;
static u16 queried_vid;

static int lut_find(const rtk_mac_t *mac, const rtk_l2_ucastAddr_t *key)
{
	int i;

	for (i = 0; i < 4; i++)
		if (lut[i].present && key->ivl == lut[i].entry.ivl &&
		    key->vid_fid == lut[i].entry.vid_fid &&
		    !memcmp(mac->octet, lut[i].entry.mac.octet, ETH_ALEN))
			return i;
	return -1;
}
static int rtk_l2_addr_get(rtk_mac_t *mac, rtk_l2_ucastAddr_t *entry)
{
	int i = lut_find(mac, entry);

	gets++;
	queried_vid = entry->vid_fid;
	if (get_error)
		return get_error;
	if (i < 0)
		return RT_ERR_L2_ENTRY_NOTFOUND;
	*entry = lut[i].entry;
	return RT_ERR_OK;
}
static int rtk_l2_addr_add(rtk_mac_t *mac, rtk_l2_ucastAddr_t *entry)
{
	int i = lut_find(mac, entry);

	adds++;
	assert(!memcmp(mac->octet, entry->mac.octet, ETH_ALEN));
	if (i < 0)
		for (i = 0; i < 4 && lut[i].present; i++) {}
	assert(i < 4);
	lut[i].present = true;
	lut[i].entry = *entry;
	return RT_ERR_OK;
}
static int rtk_l2_addr_del(rtk_mac_t *mac, rtk_l2_ucastAddr_t *entry)
{
	int i = lut_find(mac, entry);

	dels++;
	if (del_error)
		return del_error;
	if (i < 0)
		return RT_ERR_L2_ENTRY_NOTFOUND;
	/* The real SDK deletes by MAC + IVL/FID, not by the supplied port. */
	lut[i].present = false;
	return RT_ERR_OK;
}
