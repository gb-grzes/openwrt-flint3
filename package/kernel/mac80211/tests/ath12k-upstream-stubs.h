/* SPDX-License-Identifier: GPL-2.0 */
/* Test only: small models for executing extracted production functions. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef struct { unsigned held; } spinlock_t;
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define unlikely(x) (x)
#define ATH12K_FLAG_CRASH_FLUSH 0
#define RPROC_RUNNING 1
#define rcu_dereference(p) (p)
#define ath12k_err(...) ((void)0)
#define lockdep_assert_held(p) assert((p)->held)

struct device { int unused; };
struct platform_device { struct device dev; };
struct rproc { int state; };
struct ath12k_ahb { struct rproc *tgt_rproc; };
struct hal_srng {
	spinlock_t lock;
	u32 ring_size, entry_size;
	union { struct { u32 hp, cached_tp; u32 *tp_addr; } src_ring; } u;
};
struct dp_srng { unsigned ring_id; };
struct dp_tx_ring { spinlock_t wake_tx_lock; struct dp_srng tcl_data_ring; };
struct ath12k_hw_ops { u8 (*get_ring_selector)(u8 ac); };
struct ath12k_hw_params { struct ath12k_hw_ops *hw_ops; unsigned max_tx_ring; };
struct ath12k_hal { struct hal_srng srng_list[4]; };
struct ath12k_dp {
	struct ath12k_hw_params *hw_params;
	struct ath12k_hal *hal;
	struct dp_tx_ring tx_ring[4];
};
struct ath12k_base {
	struct platform_device *pdev;
	struct ath12k_ahb *ahb;
	struct ath12k_dp *dp;
	unsigned long dev_flags;
};
struct ath12k { struct ath12k_base *ab; };
struct ath12k_link_vif { struct ath12k *ar; u8 link_id; };
struct ath12k_vif { struct ath12k_link_vif deflink, *link[18]; };
struct ieee80211_vif { bool mld; struct ath12k_vif *priv; };
struct ath12k_sta { u8 assoc_link_id; };
struct ieee80211_sta { struct ath12k_sta *priv; };
struct ieee80211_txq {
	struct ieee80211_vif *vif;
	struct ieee80211_sta *sta;
	u8 ac;
};
struct ieee80211_hw { int unused; };
struct ieee80211_tx_control { struct ieee80211_sta *sta; };
struct sk_buff { int unused; };

static struct {
	int get_ret, notifier_ret, boot_ret, irq_ret;
	unsigned refs, notifiers, puts, unregisters, boots, irq_calls;
	unsigned rcu_depth, bh_depth, locks, unlocks, queued, sent, dequeues;
	unsigned cpu, selector_calls, ac, ring_id;
	bool crash_on_tx;
	struct ath12k_base *tx_ab;
	struct dp_tx_ring *tx_ring;
	struct hal_srng *tcl_ring;
	struct ieee80211_sta *expected_sta;
} test_state;

static struct ath12k_ahb *ath12k_ab_to_ahb(struct ath12k_base *ab) { return ab->ahb; }
static int ath12k_ahb_get_rproc(struct ath12k_base *ab)
{
	(void)ab;
	if (!test_state.get_ret) test_state.refs++;
	return test_state.get_ret;
}
static int ath12k_ahb_register_rproc_notifier(struct ath12k_base *ab)
{
	(void)ab;
	if (!test_state.notifier_ret) test_state.notifiers++;
	return test_state.notifier_ret;
}
static void ath12k_ahb_unregister_rproc_notifier(struct ath12k_base *ab)
{
	(void)ab;
	assert(test_state.notifiers == 1);
	test_state.notifiers--;
	test_state.unregisters++;
}
static int ath12k_ahb_boot_root_pd(struct ath12k_base *ab)
{
	test_state.boots++;
	if (!test_state.boot_ret) ab->ahb->tgt_rproc->state = RPROC_RUNNING;
	return test_state.boot_ret;
}
static int ath12k_ahb_config_rproc_irq(struct ath12k_base *ab)
{
	(void)ab;
	test_state.irq_calls++;
	return test_state.irq_ret;
}
static int dev_err_probe(struct device *dev, int ret, const char *fmt)
{
	(void)dev; (void)fmt;
	return ret;
}
static void rproc_put(struct rproc *rproc)
{
	(void)rproc;
	assert(test_state.refs == 1);
	test_state.refs--;
	test_state.puts++;
}
static void rcu_read_lock(void) { assert(!test_state.rcu_depth); test_state.rcu_depth++; }
static void rcu_read_unlock(void) { assert(test_state.rcu_depth == 1); test_state.rcu_depth--; }
static bool test_bit(unsigned bit, const unsigned long *flags) { return (*flags >> bit) & 1; }
static struct ath12k_vif *ath12k_vif_to_ahvif(struct ieee80211_vif *vif) { return vif->priv; }
static struct ath12k_sta *ath12k_sta_to_ahsta(struct ieee80211_sta *sta) { return sta->priv; }
static bool ieee80211_vif_is_mld(struct ieee80211_vif *vif) { return vif->mld; }
static void spin_lock_bh(spinlock_t *lock)
{
	assert(!lock->held);
	lock->held = 1;
	test_state.bh_depth++;
	test_state.locks++;
}
static void spin_unlock_bh(spinlock_t *lock)
{
	assert(lock->held && test_state.bh_depth);
	lock->held = 0;
	test_state.bh_depth--;
	test_state.unlocks++;
}
typedef struct { spinlock_t *lock; } test_guard_t;
static test_guard_t test_guard_lock(spinlock_t *lock)
{
	spin_lock_bh(lock);
	return (test_guard_t){ lock };
}
static void test_guard_unlock(test_guard_t *guard) { spin_unlock_bh(guard->lock); }
#define guard(name) test_guard_t __attribute__((cleanup(test_guard_unlock))) test_guard = test_guard_lock
#define smp_processor_id() test_state.cpu

static struct sk_buff *ieee80211_tx_dequeue(struct ieee80211_hw *hw,
					 struct ieee80211_txq *txq)
{
	static struct sk_buff skb;
	(void)hw; (void)txq;
	assert(test_state.rcu_depth && test_state.bh_depth);
	assert(test_state.tx_ring->wake_tx_lock.held);
	assert(!test_state.tcl_ring->lock.held);
	test_state.dequeues++;
	if (!test_state.queued) return NULL;
	test_state.queued--;
	return &skb;
}
static void ath12k_wifi7_mac_op_tx(struct ieee80211_hw *hw,
				 struct ieee80211_tx_control *control,
				 struct sk_buff *skb)
{
	struct hal_srng *ring = test_state.tcl_ring;
	(void)hw; (void)skb;
	assert(control->sta == test_state.expected_sta);
	assert(test_state.rcu_depth && test_state.bh_depth);
	assert(test_state.tx_ring->wake_tx_lock.held && !ring->lock.held);
	assert(ring->u.src_ring.cached_tp !=
	       (ring->u.src_ring.hp + ring->entry_size) % ring->ring_size);
	ring->u.src_ring.hp = (ring->u.src_ring.hp + ring->entry_size) % ring->ring_size;
	test_state.sent++;
	if (test_state.crash_on_tx) test_state.tx_ab->dev_flags = 1;
}
