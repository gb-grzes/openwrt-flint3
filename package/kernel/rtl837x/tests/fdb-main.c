/* SPDX-License-Identifier: GPL-2.0 */
static const unsigned char client[ETH_ALEN] = { 0x60, 0x57, 0x18, 0x1a, 0xf6, 0x67 };

static void reset_lut(void)
{
	memset(lut, 0, sizeof(lut));
	gets = adds = dels = 0;
	get_error = del_error = 0;
	lut[0].present = true;
	memcpy(lut[0].entry.mac.octet, client, ETH_ALEN);
	lut[0].entry.ivl = 1;
	lut[0].entry.vid_fid = 1;
	lut[0].entry.port = 2;
	lut[1] = lut[0];
	lut[1].entry.mac.octet[5]++;
	lut[2] = lut[0];
	lut[2].entry.vid_fid = 10;
}

int main(void)
{
	int bridge_a, bridge_b;
	struct rtk_gsw gsw = {
		.chip_id = CHIP_RTL8372N, .cpu_port = 8,
		.tag_proto = DSA_TAG_PROTO_RTL8_4,
		.valid_port_mask = BIT(2) | BIT(3) | BIT(8),
	};
	struct dsa_port dp = { .index = 2 };
	struct dsa_db db = {
		.type = DSA_DB_BRIDGE,
		.bridge = { .dev = &bridge_a, .num = 1 },
	};
	struct dsa_db port_db = { .type = DSA_DB_PORT, .dp = &dp };
	struct dsa_db bad_db = { .type = DSA_DB_LAG };
	u16 vid;

	gsw.ds.priv = &gsw;
	gsw.bridge_dev[2] = &bridge_a;
	assert(!rtl837x_fdb_vid(&gsw, 0, db, &vid) && vid == 1);
	assert(!rtl837x_fdb_vid(&gsw, 0, port_db, &vid) && vid == 1);
	assert(!rtl837x_fdb_vid(&gsw, 10, db, &vid) && vid == 10);
	assert(rtl837x_fdb_vid(&gsw, 0, bad_db, &vid) == -EOPNOTSUPP);
	puts("PASS: native VID0 maps to VLAN1; explicit VIDs and unsupported DB checks remain.");

	gsw.tag_proto = DSA_TAG_PROTO_VSC73XX_8021Q;
	assert(!rtl837x_fdb_vid(&gsw, 0, db, &vid) && vid == 201);
	assert(!rtl837x_fdb_vid(&gsw, 0, port_db, &vid) && vid == 102);
	assert(!rtl837x_fdb_vid(&gsw, 10, db, &vid) && vid == 10);
	reset_lut();
	lut[0].entry.vid_fid = 201;
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db));
	assert(!lut[0].present && queried_vid == 201 && dels == 1);
	puts("PASS: fallback tag_8021q keeps its DSA database selection.");
	gsw.tag_proto = DSA_TAG_PROTO_RTL8_4;

	reset_lut();
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db));
	assert(gets == 1 && dels == 1 && adds == 0 && !gsw.isolation_lock);
	assert(!lut[0].present && lut[1].present && lut[2].present);
	puts("PASS: foreign bridge add removes just the stale dynamic LAN key, without a CPU entry.");
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db));
	assert(gets == 2 && dels == 1 && !gsw.isolation_lock);
	puts("PASS: repeated notifications and missing entries are harmless.");

	reset_lut();
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 10, db));
	assert(lut[0].present && lut[1].present && !lut[2].present && queried_vid == 10);
	puts("PASS: explicit VLAN delete does not touch the same MAC in a different VLAN.");

	reset_lut();
	lut[0].entry.is_static = 1;
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db));
	assert(lut[0].present && !dels);
	puts("PASS: user static entries are preserved.");

	reset_lut();
	gsw.bridge_dev[2] = &bridge_b;
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db));
	assert(lut[0].present && !dels);
	gsw.bridge_dev[2] = NULL;
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db));
	assert(lut[0].present && !dels);
	gsw.bridge_dev[2] = &bridge_a;
	puts("PASS: other bridges and standalone ports are not cleared.");

	reset_lut();
	lut[0].entry.port = 8;
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db));
	assert(lut[0].present && !dels);
	lut[0].entry.port = 15;
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db));
	assert(lut[0].present && !dels);
	puts("PASS: CPU and invalid-port entries are preserved.");

	reset_lut();
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, port_db));
	assert(lut[0].present && !gets && !dels && !adds);
	puts("PASS: private host addresses remain on the existing CPU no-op path.");
	assert(!rtl837x_port_fdb_del(&gsw.ds, 8, client, 0, db));
	assert(lut[0].present && !gets && !dels && !adds);
	puts("PASS: CPU delete cannot erase a relearned returning LAN client.");

	reset_lut();
	get_error = RT_ERR_BUSYWAIT_TIMEOUT;
	assert(rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db) == -ETIMEDOUT);
	assert(lut[0].present && !dels && !gsw.isolation_lock);
	get_error = 0;
	del_error = RT_ERR_BUSYWAIT_TIMEOUT;
	assert(rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db) == -ETIMEDOUT);
	assert(lut[0].present && !gsw.isolation_lock);
	del_error = RT_ERR_L2_ENTRY_NOTFOUND;
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db));
	assert(!gsw.isolation_lock);
	puts("PASS: SDK errors propagate, disappearance is tolerated, and locks are released.");

	reset_lut();
	assert(rtl837x_port_fdb_add(&gsw.ds, 15, client, 0, db) == -EINVAL);
	assert(rtl837x_port_fdb_del(&gsw.ds, -1, client, 0, db) == -EINVAL);
	assert(!gets && !dels && !adds);
	assert(!rtl837x_port_fdb_add(&gsw.ds, 2, client, 0, db));
	assert(lut[0].present && lut[0].entry.is_static && lut[0].entry.auth);
	assert(lut[0].entry.vid_fid == 1 && adds == 1);
	assert(!rtl837x_port_fdb_del(&gsw.ds, 2, client, 0, db));
	assert(!lut[0].present && dels == 1);
	puts("PASS: user-port add/delete use symmetric native keys; invalid ports are rejected.");

	reset_lut();
	gsw.chip_id = CHIP_RTL8373;
	assert(!rtl837x_port_fdb_add(&gsw.ds, 8, client, 0, db));
	assert(adds == 1 && !gets && !dels);
	assert(lut[0].entry.port == 8 && lut[0].entry.is_static);
	puts("PASS: the RTL8372N CPU quirk does not replace other chips' normal add path.");
	return 0;
}
