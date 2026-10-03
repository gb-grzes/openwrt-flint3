/* SPDX-License-Identifier: GPL-2.0 */
/* Host-only netlink/UCI model. No kernel or router access. */
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __IWINFO_UTILS_H_ /* Do not pull target-only UCI/ubus headers into host tests. */
#include "iwinfo.h"
#include "api/nl80211.h"
static int iwinfo_dbm2mw(int in);

#define NL_SKIP 2
#define NLM_F_DUMP 0x300
#define NLA_U32 1
#define NLA_FLAG 2
struct nla_policy { int type; };
struct nlattr {
	unsigned int type;
	uint32_t value;
	unsigned int count;
	struct nlattr *children[NL80211_ATTR_MAX + 1];
};
struct nl_msg { struct nlattr *root; };
struct nl80211_msg_conveyor { struct nl_msg *msg; };
struct uci_section { const char *band, *radio; };
static struct uci_section uci_radios[3] = {
	{ "2g", "0" }, { "5g", "1" }, { "6g", "2" }
};
static void *uci_ctx;
static unsigned int uci_frees;
static struct nlattr pool[512];
static unsigned int used;
static struct nl_msg interface_msgs[8], wiphy_msgs[8];
static unsigned int n_interfaces, n_wiphys;
static int interface_error, send_error, put_error;
static int interface_flags, wiphy_flags;
static bool split_requested;
static unsigned int frees, sends;
static uint32_t protocol_features = NL80211_PROTOCOL_FEATURE_SPLIT_WIPHY_DUMP;

static struct nlattr *attr(unsigned int type, uint32_t value)
{
	struct nlattr *a;

	assert(used < sizeof(pool) / sizeof(pool[0]));
	a = &pool[used++];
	a->type = type;
	a->value = value;
	return a;
}

static void set(struct nlattr *root, unsigned int type, uint32_t value)
{
	root->children[type] = attr(type, value);
}

static struct nlattr *nested(struct nlattr *root, unsigned int type)
{
	return root->children[type] = attr(type, 0);
}

static struct nlattr *append(struct nlattr *list, unsigned int type)
{
	assert(list->count <= NL80211_ATTR_MAX);
	return list->children[list->count++] = attr(type, 0);
}

#define nla_for_each_nested(a, parent, rem) \
	for ((rem) = 0; (parent) && (unsigned int)(rem) < (parent)->count && \
	     ((a) = (parent)->children[rem]); (rem)++)
static uint32_t nla_get_u32(struct nlattr *a) { assert(a); return a->value; }
static void *nla_data(struct nlattr *a) { return a; }
static int nla_len(struct nlattr *a) { return a->count; }
static int nla_parse(struct nlattr **tb, int max, void *data, int len,
		     const struct nla_policy *policy)
{
	struct nlattr *a = data;
	(void)len;
	(void)policy;
	memcpy(tb, a->children, (max + 1) * sizeof(*tb));
	return 0;
}
static int nla_parse_nested(struct nlattr **tb, int max, struct nlattr *a,
			    const struct nla_policy *policy)
{
	return nla_parse(tb, max, a, 0, policy);
}
static struct nlattr **nl80211_parse(struct nl_msg *msg) { return msg->root->children; }
static int iwinfo_mbm2dbm(int gain) { return gain / 100; }

static struct uci_section *iwinfo_uci_get_radio(const char *name, const char *type)
{
	(void)type;
	if (!strncmp(name, "radio", 5) && name[5] >= '0' && name[5] <= '2' && !name[6])
		return &uci_radios[name[5] - '0'];
	return NULL;
}
static const char *uci_lookup_option_string(void *ctx, struct uci_section *s,
					   const char *option)
{
	(void)ctx;
	return !strcmp(option, "band") ? s->band : s->radio;
}
static void iwinfo_uci_free(void) { uci_frees++; }
static char *nl80211_phy2ifname(const char *name)
{
	return !strcmp(name, "phy0") ? "ap-mld0" : NULL;
}
static int nl80211_request(const char *name, int cmd, int flags,
			   int (*cb)(struct nl_msg *, void *), void *data)
{
	unsigned int i;
	(void)name;
	assert(cmd == NL80211_CMD_GET_INTERFACE);
	interface_flags = flags;
	if (interface_error)
		return interface_error;
	for (i = 0; i < n_interfaces; i++)
		cb(&interface_msgs[i], data);
	return 0;
}
static int nl80211_get_protocol_features(const char *name)
{
	(void)name;
	return protocol_features;
}
static struct nl80211_msg_conveyor *nl80211_msg(const char *name, int cmd, int flags)
{
	static struct nl_msg msg;
	static struct nl80211_msg_conveyor cv = { &msg };
	(void)name;
	assert(cmd == NL80211_CMD_GET_WIPHY);
	wiphy_flags = flags;
	return &cv;
}
#define NLA_PUT_FLAG(msg, type) do { \
	(void)(msg); assert((type) == NL80211_ATTR_SPLIT_WIPHY_DUMP); \
	if (put_error) goto nla_put_failure; \
	split_requested = true; \
} while (0)
static void nl80211_free(struct nl80211_msg_conveyor *cv) { (void)cv; frees++; }
static int nl80211_send(struct nl80211_msg_conveyor *cv,
			int (*cb)(struct nl_msg *, void *), void *data)
{
	unsigned int i;
	sends++;
	/* Production nl80211_send() consumes its conveyor on every return path. */
	nl80211_free(cv);
	if (send_error)
		return send_error;
	for (i = 0; i < n_wiphys; i++)
		cb(&wiphy_msgs[i], data);
	return 0;
}
