/* SPDX-License-Identifier: GPL-2.0 */
static void reset(void)
{
	memset(pool, 0, sizeof(pool));
	used = n_interfaces = n_wiphys = 0;
	interface_error = send_error = put_error = 0;
	interface_flags = wiphy_flags = -1;
	split_requested = false;
	frees = sends = uci_frees = 0;
	protocol_features = NL80211_PROTOCOL_FEATURE_SPLIT_WIPHY_DUMP;
}

static struct nlattr *interface(unsigned int mask)
{
	struct nlattr *root = attr(0, 0);

	set(root, NL80211_ATTR_VIF_RADIO_MASK, mask);
	interface_msgs[n_interfaces++].root = root;
	return root;
}

static void sample(struct nlattr *root, int frequency, int mbm)
{
	if (frequency)
		set(root, NL80211_ATTR_WIPHY_FREQ, frequency);
	if (mbm != INT_MIN)
		set(root, NL80211_ATTR_WIPHY_TX_POWER_LEVEL, mbm);
}

static struct nlattr *wiphy_frequency(int frequency, int mbm)
{
	struct nlattr *root = attr(0, 0);
	struct nlattr *bands = nested(root, NL80211_ATTR_WIPHY_BANDS);
	struct nlattr *band = append(bands, NL80211_BAND_5GHZ);
	struct nlattr *freqs = nested(band, NL80211_BAND_ATTR_FREQS);
	struct nlattr *freq = append(freqs, 1);

	set(freq, NL80211_FREQUENCY_ATTR_FREQ, frequency);
	if (mbm != INT_MIN)
		set(freq, NL80211_FREQUENCY_ATTR_MAX_TX_POWER, mbm);
	wiphy_msgs[n_wiphys++].root = root;
	return freq;
}

static struct nlattr *mld(int power5, int power6)
{
	struct nlattr *links = nested(interface(6), NL80211_ATTR_MLO_LINKS);
	struct nlattr *link = append(links, 2);

	set(link, NL80211_ATTR_MLO_LINK_ID, 1);
	sample(link, 5180, power5);
	link = append(links, 3);
	set(link, NL80211_ATTR_MLO_LINK_ID, 2);
	sample(link, 6055, power6);
	return links;
}

static void expect_list(const char *name, int max)
{
	union { char raw[IWINFO_BUFSIZE]; struct iwinfo_txpwrlist_entry entries[49]; } buf;
	int len = -1, i;

	assert(!nl80211_get_txpwrlist(name, buf.raw, &len));
	assert(len == (max + 1) * (int)sizeof(buf.entries[0]));
	for (i = 0; i <= max; i++) {
		assert(buf.entries[i].dbm == i);
		assert(buf.entries[i].mw == iwinfo_dbm2mw(i));
	}
	assert(split_requested && wiphy_flags == NLM_F_DUMP);
	assert(sends == 1 && frees == 1);
}

static void expect_no_list(const char *name)
{
	char buf[IWINFO_BUFSIZE];
	int len = 999;

	assert(nl80211_get_txpwrlist(name, buf, &len) == -1);
	assert(len == 0);
}

int main(void)
{
	struct nl80211_power_data data;
	struct nlattr *root, *freq;
	const char *saved;
	int dbm = 1234, i;
	const int frequency[] = { 2442, 5180, 6055 };
	const int power[] = { 1600, 2200, 2200 };
	const char *radio[] = { "radio0", "radio1", "radio2" };

	/* All three UCI radios share phy0; netdev order must not select the band. */
	for (i = 0; i < 3; i++) {
		reset();
		mld(2200, 2200);
		sample(interface(4), 6055, 2200);
		sample(interface(1), 2442, 1600);
		sample(interface(2), 5180, 2200);
		assert(!nl80211_get_txpower(radio[i], &dbm));
		assert(dbm == power[i] / 100 && interface_flags == NLM_F_DUMP);
		wiphy_frequency(frequency[i], power[i]);
		expect_list(radio[i], power[i] / 100);
	}
	puts("PASS: all three radios on one wiphy, including MLD-first ordering");

	reset();
	sample(interface(2), 5180, 2200);
	assert(!nl80211_get_txpower("phy0.1-ap0", &dbm) && dbm == 22);
	assert(interface_flags == 0);
	puts("PASS: ordinary interface reports 22 dBm");

	reset();
	root = interface(2);
	sample(root, 5180, INT_MIN);
	dbm = 1234;
	assert(nl80211_get_txpower("phy0.1-ap0", &dbm) == -1 && dbm == 1234);
	set(root, NL80211_ATTR_WIPHY_TX_POWER_LEVEL, 0);
	assert(!nl80211_get_txpower("phy0.1-ap0", &dbm) && dbm == 0);
	set(root, NL80211_ATTR_WIPHY_TX_POWER_LEVEL, (uint32_t)-100);
	assert(!nl80211_get_txpower("phy0.1-ap0", &dbm) && dbm == -1);
	puts("PASS: missing power is unknown; genuine zero and signed mBm survive");

	reset();
	mld(2200, 2200);
	assert(!nl80211_get_txpower("ap-mld0", &dbm) && dbm == 22);
	assert(!nl80211_get_power_data("ap-mld0", &data) && data.frequency == 0);
	expect_no_list("ap-mld0");
	puts("PASS: equal MLO link powers; no invented cross-band frequency");

	reset();
	mld(1700, 2200);
	assert(nl80211_get_txpower("ap-mld0", &dbm) == -1);
	assert(!nl80211_get_txpower("radio1", &dbm) && dbm == 17);
	assert(!nl80211_get_txpower("radio2", &dbm) && dbm == 22);
	wiphy_frequency(5180, 2300);
	expect_list("radio1", 23);
	puts("PASS: MLO-only setup selects band; ambiguous MLD power is unknown");

	reset();
	mld(2200, INT_MIN);
	assert(nl80211_get_txpower("ap-mld0", &dbm) == -1);
	assert(nl80211_get_txpower("radio2", &dbm) == -1);
	assert(!nl80211_get_txpower("radio1", &dbm) && dbm == 22);
	puts("PASS: missing MLO link power is not replaced by another link");

	reset();
	sample(interface(1), 2412, 1700);
	wiphy_frequency(2412, 2000);
	wiphy_frequency(5955, 3000); /* Both frequencies are channel 1. */
	expect_list("radio0", 20);
	puts("PASS: frequency matching separates overlapping 2.4/6 GHz channels");

	reset();
	sample(interface(2), 5180, 2200);
	wiphy_msgs[n_wiphys++].root = attr(0, 0);
	root = attr(0, 0);
	append(nested(root, NL80211_ATTR_WIPHY_BANDS), 0);
	wiphy_msgs[n_wiphys++].root = root; /* Capabilities without frequencies. */
	wiphy_frequency(5180, 2300);
	wiphy_frequency(6055, 2200);
	wiphy_msgs[n_wiphys++].root = attr(0, 0);
	expect_list("radio1", 23);
	puts("PASS: split wiphy fragments retain the matched limit");

	reset();
	sample(interface(2), 5180, 2200);
	expect_no_list("radio1");
	wiphy_frequency(5180, INT_MIN);
	expect_no_list("radio1");
	puts("PASS: absent limits yield an empty list, never 255 dBm");

	reset();
	sample(interface(2), 5180, 2200);
	freq = wiphy_frequency(5180, 2300);
	set(freq, NL80211_FREQUENCY_ATTR_DISABLED, 1);
	expect_no_list("radio1");
	freq->children[NL80211_FREQUENCY_ATTR_DISABLED] = NULL;
	set(freq, NL80211_FREQUENCY_ATTR_MAX_TX_POWER, 4900);
	expect_no_list("radio1");
	set(freq, NL80211_FREQUENCY_ATTR_MAX_TX_POWER, (uint32_t)-100);
	expect_no_list("radio1");
	set(freq, NL80211_FREQUENCY_ATTR_MAX_TX_POWER, (uint32_t)-50);
	expect_no_list("radio1");
	puts("PASS: disabled, overflowing and negative limits are rejected");

	reset();
	sample(interface(2), 5180, 0);
	wiphy_frequency(5180, 0);
	expect_list("radio1", 0);
	reset();
	sample(interface(2), 5180, 2200);
	wiphy_frequency(5180, 4800);
	expect_list("radio1", 48);
	puts("PASS: zero limit and largest representable whole-dBm limit");

	reset();
	interface_error = -1;
	assert(nl80211_get_txpower("radio1", &dbm) == -1);
	expect_no_list("radio1");
	assert(!sends && !frees);
	reset();
	sample(interface(2), 5180, 2200);
	put_error = -1;
	expect_no_list("radio1");
	assert(!sends && frees == 1);
	reset();
	sample(interface(2), 5180, 2200);
	send_error = -1;
	expect_no_list("radio1");
	assert(sends == 1 && frees == 1);
	puts("PASS: request errors and conveyor ownership, including put failure");

	reset();
	sample(interface(2), 5180, 2200);
	saved = uci_radios[1].radio;
	uci_radios[1].radio = "32";
	assert(nl80211_get_txpower("radio1", &dbm) == -1);
	assert(uci_frees == 1 && interface_flags == -1);
	uci_radios[1].radio = saved;
	puts("PASS: invalid radio index rejected before shift/request");

	reset();
	sample(interface(4), 5180, 1900); /* Matching band, wrong physical radio. */
	assert(nl80211_get_txpower("radio1", &dbm) == -1);
	puts("PASS: physical radio mask excludes the wrong radio");

	reset();
	sample(interface(2), 5180, 2200);
	wiphy_frequency(5180, 2300);
	wiphy_frequency(5180, 2000);
	expect_list("radio1", 20);
	reset();
	sample(interface(2), 5180, 2200);
	wiphy_frequency(5180, (uint32_t)-100);
	wiphy_frequency(5180, 2300);
	expect_no_list("radio1");
	puts("PASS: duplicate frequency limits retain the most restrictive value");

	reset();
	mld(2200, 2200);
	saved = uci_radios[1].band;
	uci_radios[1].band = NULL;
	assert(nl80211_get_txpower("radio1", &dbm) == -1);
	uci_radios[1].band = saved;
	puts("PASS: MLD radio query without a band is not guessed");

	puts("All TX-power regression tests passed.");
	return 0;
}
