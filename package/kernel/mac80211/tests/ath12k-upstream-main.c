/* SPDX-License-Identifier: GPL-2.0 */
static struct {
	struct platform_device pdev;
	struct rproc rproc;
	struct ath12k_ahb ahb;
	struct ath12k_base ab;
	struct ath12k ar;
	struct ath12k_vif ahvif;
	struct ieee80211_vif vif;
	struct ath12k_sta ahsta;
	struct ieee80211_sta sta;
	struct ieee80211_txq txq;
	struct ieee80211_hw hw;
	struct ath12k_hw_ops ops;
	struct ath12k_hw_params params;
	struct ath12k_dp dp;
	struct ath12k_hal hal;
	u32 tail[4];
} fixture;
static unsigned groups;

static u8 test_selector(u8 ac)
{
	u8 selector = ath12k_wifi7_hw_get_ring_selector_qcn9274(ac);
	test_state.ac = ac;
	test_state.selector_calls++;
	test_state.ring_id = selector % fixture.params.max_tx_ring;
	test_state.tx_ring = &fixture.dp.tx_ring[test_state.ring_id];
	test_state.tcl_ring = &fixture.hal.srng_list[test_state.ring_id];
	return selector;
}
static void reset_fixture(void)
{
	memset(&fixture, 0, sizeof(fixture));
	memset(&test_state, 0, sizeof(test_state));
	fixture.ahb.tgt_rproc = &fixture.rproc;
	fixture.ab = (struct ath12k_base){ .pdev = &fixture.pdev, .ahb = &fixture.ahb,
		.dp = &fixture.dp };
	fixture.ar.ab = &fixture.ab;
	fixture.ahvif.deflink = (struct ath12k_link_vif){ .ar = &fixture.ar };
	fixture.ahvif.link[0] = &fixture.ahvif.deflink;
	fixture.vif.priv = &fixture.ahvif;
	fixture.sta.priv = &fixture.ahsta;
	fixture.txq.vif = &fixture.vif;
	fixture.txq.ac = 2;
	fixture.ops.get_ring_selector = test_selector;
	fixture.params = (struct ath12k_hw_params){ .hw_ops = &fixture.ops, .max_tx_ring = 4 };
	fixture.dp.hw_params = &fixture.params;
	fixture.dp.hal = &fixture.hal;
	test_state.tx_ab = &fixture.ab;
	for (unsigned i = 0; i < 4; i++) {
		fixture.dp.tx_ring[i].tcl_data_ring.ring_id = i;
		fixture.hal.srng_list[i].ring_size = 64;
		fixture.hal.srng_list[i].entry_size = 4;
		fixture.hal.srng_list[i].u.src_ring.tp_addr = &fixture.tail[i];
	}
}
static void set_capacity(unsigned free_slots)
{
	assert(free_slots < 16);
	for (unsigned i = 0; i < 4; i++) fixture.tail[i] = ((free_slots + 1) * 4) % 64;
}
static void wake_queue(void)
{
	ath12k_wifi7_mac_op_wake_tx_queue(&fixture.hw, &fixture.txq);
	assert(!test_state.rcu_depth && !test_state.bh_depth);
	assert(test_state.locks == test_state.unlocks);
	for (unsigned i = 0; i < 4; i++) {
		assert(!fixture.dp.tx_ring[i].wake_tx_lock.held);
		assert(!fixture.hal.srng_list[i].lock.held);
	}
}
static void passed(const char *name) { groups++; printf("ok %u - %s\n", groups, name); }

int main(void)
{
	reset_fixture(); test_state.get_ret = -ENODEV;
	assert(ath12k_ahb_configure_rproc(&fixture.ab) == -ENODEV);
	assert(!test_state.puts && !test_state.unregisters && !test_state.irq_calls);
	passed("rproc lookup failure does not release unowned resources");
	reset_fixture(); test_state.notifier_ret = -ENOMEM;
	assert(ath12k_ahb_configure_rproc(&fixture.ab) == -ENOMEM);
	assert(test_state.puts == 1 && !test_state.unregisters && !test_state.refs);
	passed("notifier failure releases the rproc reference once");
	reset_fixture(); test_state.boot_ret = -EIO;
	assert(ath12k_ahb_configure_rproc(&fixture.ab) == -EIO);
	assert(test_state.puts == 1 && test_state.unregisters == 1 && !test_state.irq_calls);
	passed("root boot failure unwinds notifier and reference");
	for (unsigned running = 0; running < 2; running++) {
		reset_fixture(); fixture.rproc.state = running ? RPROC_RUNNING : 0;
		test_state.irq_ret = -EINVAL;
		assert(ath12k_ahb_configure_rproc(&fixture.ab) == -EINVAL);
		assert(test_state.puts == 1 && test_state.unregisters == 1);
		assert(!test_state.refs && !test_state.notifiers && test_state.irq_calls == 1);
		assert(test_state.boots == !running);
	}
	passed("IRQ failure unwinds both new and already-running rproc paths");
	for (unsigned running = 0; running < 2; running++) {
		reset_fixture(); fixture.rproc.state = running ? RPROC_RUNNING : 0;
		assert(ath12k_ahb_configure_rproc(&fixture.ab) == 0);
		assert(test_state.refs == 1 && test_state.notifiers == 1);
		assert(!test_state.puts && !test_state.unregisters && test_state.boots == !running);
	}
	passed("successful rproc initialization retains ownership");
	reset_fixture(); test_state.cpu = 7;
	assert(ath12k_wifi7_hw_get_ring_selector_qcn9274(0) == 7);
	assert(ath12k_wifi7_hw_get_ring_selector_qcn9274(3) == 7);
	for (u8 ac = 0; ac < 4; ac++) assert(ath12k_wifi7_hw_get_ring_selector_wcn7850(ac) == ac);
	passed("ring-selector prerequisite preserves CPU and AC behavior");
	reset_fixture(); set_capacity(4); test_state.queued = 2; test_state.cpu = 7;
	wake_queue(); assert(test_state.sent == 2 && test_state.dequeues == 3);
	assert(test_state.ring_id == 3 && test_state.ac == 2);
	assert(fixture.hal.srng_list[3].u.src_ring.cached_tp == fixture.tail[3]);
	passed("queue drains available frames with balanced locks and hardware-tail synchronization");
	reset_fixture(); set_capacity(0); test_state.queued = 4;
	wake_queue(); assert(!test_state.sent && !test_state.dequeues && test_state.queued == 4);
	passed("full TCL ring leaves all frames queued in mac80211");
	reset_fixture(); set_capacity(2); test_state.queued = 5;
	wake_queue(); assert(test_state.sent == 2 && test_state.dequeues == 2 && test_state.queued == 3);
	passed("queue stops exactly when the selected ring fills");
	fixture.tail[0] = 24;
	wake_queue(); assert(test_state.sent == 5 && !test_state.queued);
	passed("later wake after simulated hardware progress resumes queued traffic");
	reset_fixture(); set_capacity(15); test_state.queued = 15;
	wake_queue(); assert(test_state.sent == 15 && !test_state.queued);
	passed("ring wrap arithmetic retains the reserved empty slot");
	reset_fixture(); fixture.vif.mld = true; fixture.txq.sta = &fixture.sta;
	fixture.ahsta.assoc_link_id = 1; fixture.ahvif.link[1] = &fixture.ahvif.deflink;
	fixture.ahvif.link[0] = NULL; test_state.expected_sta = &fixture.sta;
	set_capacity(3); test_state.queued = 1; wake_queue(); assert(test_state.sent == 1);
	passed("MLD station queue uses association link 1 without requiring link 0");
	reset_fixture(); fixture.vif.mld = true; fixture.ahvif.deflink.link_id = 2;
	fixture.ahvif.link[2] = &fixture.ahvif.deflink; fixture.ahvif.link[0] = NULL;
	set_capacity(3); test_state.queued = 1; wake_queue(); assert(test_state.sent == 1);
	passed("MLD non-station queue uses default link 2 without requiring link 0");
	reset_fixture(); fixture.ahvif.deflink.link_id = 255;
	wake_queue(); assert(!test_state.selector_calls && !test_state.dequeues);
	reset_fixture(); fixture.ahvif.link[0] = NULL; wake_queue();
	assert(!test_state.selector_calls && !test_state.dequeues);
	reset_fixture(); fixture.ahvif.deflink.ar = NULL; wake_queue();
	assert(!test_state.selector_calls && !test_state.dequeues);
	passed("invalid link ID, missing link and missing radio do not touch ring memory");
	reset_fixture(); fixture.ab.dev_flags = 1; fixture.ab.dp = NULL;
	wake_queue(); assert(!test_state.selector_calls && !test_state.dequeues);
	reset_fixture(); fixture.ab.dp = NULL; wake_queue();
	assert(!test_state.selector_calls && !test_state.dequeues);
	passed("crash-flush and missing datapath exit before ring lookup");
	reset_fixture(); set_capacity(4); test_state.queued = 3; test_state.crash_on_tx = true;
	wake_queue(); assert(test_state.sent == 1 && test_state.queued == 2);
	passed("recovery beginning during draining stops further dequeue");
	printf("PASS: %u ath12k upstream regression groups\n", groups);
	return 0;
}
