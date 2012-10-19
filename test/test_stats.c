#include "test.h"

static void _stats_setup() {
	utils_stats_setup();
	option_parse_args(0, NULL, NULL);
	option_parse_config_file(NULL, NULL, 0, NULL);
	test(evs_server_init());
	test(apps_run());
	test(stats_init());
}

START_TEST(test_stats_sane_tick) {
	STATS_INC(clients);
	STATS_INC(clients_new);
	
	test_size_eq(stats.clients, 1, "Clients set");
	test_size_eq(stats.clients_new, 1, "Connections set");
	
	stats_flush();
	
	test_size_eq(stats.clients, 1, "Clients not reset");
	test_size_eq(stats.clients_new, 0, "Connections reset");
}
END_TEST

static gpointer _stats_tick(gpointer nothing) {
	usleep(MS_TO_USEC(100));
	
	stats_flush();
	
	return NULL;
}

START_TEST(test_stats_sane_tick_graphite) {
	STATS_INC(clients);
	STATS_INC(clients_new);
	
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	
	gchar port[6];
	snprintf(port, sizeof(port), "%d", option_stats_graphite_port());
	
	test(getaddrinfo(option_stats_graphite_address(), port, &hints, &res) == 0);
	int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	test(sock != -1);
	test(bind(sock, res->ai_addr, res->ai_addrlen) != -1);
	
	// Put the tick into a new thread so that the recvfrom will get something
	g_thread_new("stats_ticker", _stats_tick, NULL);
	
	gchar buff[8192];
	memset(&buff, 0, sizeof(buff));
	
	for (int i = 0; i < 3; i++) {
		gchar b[4096];
		memset(&b, 0, sizeof(b));
		recvfrom(sock, b, sizeof(b)-1, 0, NULL, 0);
		strcat(buff, b);
	}
	
	gchar **messages = g_strsplit(buff, "\n", -1);
	
	guint64 field_count = 7 * option_apps_count(); // For the stats in app/test
	#define X(slot, name) field_count++;
		STATS_S_COUNTERS
		STATS_S_VALUES
	#undef X
	
	guint messages_len = g_strv_length(messages)-1 ;
	
	// Empty string as the last result, but still valid
	test_uint64_eq(messages_len, field_count, "Got all the fields");
	
	// BAHAHAHAHAHA, use regex to match lines.  OHHHH man, this is awesome
	GRegex* pattern = g_regex_new("qio\\..* \\d+ \\d+", G_REGEX_OPTIMIZE, 0, NULL);
	
	for (uint i = 0; i < messages_len; i++) {
		test(g_regex_match(pattern, *(messages + i), 0, NULL), "Matches");
	}
	
	g_regex_unref(pattern);
	g_strfreev(messages);
}
END_TEST

Suite* stats_suite() {
	TCase *tc;
	Suite *s = suite_create("Stats");
	
	tc = tcase_create("Sanity");
	tcase_add_checked_fixture(tc, _stats_setup, NULL);
	tcase_add_test(tc, test_stats_sane_tick);
	tcase_add_test(tc, test_stats_sane_tick_graphite);
	suite_add_tcase(s, tc);
	
	return s;
}