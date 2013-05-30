#include "test.h"

#define CONFIG_FILE "qio.ini"
#define CONFIG_NO_GRAPHITE "[quick.io]\n" \
	"stats-graphite-address = \n" \
	"[quick.io-apps]\n"

static void _stats_setup()
{
	utils_stats_setup();
	option_parse_args(0, NULL, NULL);
	option_parse_config_file(NULL, NULL, 0, NULL);
	check(evs_server_init());
	check(apps_run());
}

START_TEST(test_stats_sane_tick)
{
	check(stats_init());

	STATS_INC(clients);
	STATS_INC(conns_new);

	check_size_eq(stats->clients, 1, "Clients set");
	check_size_eq(stats_clients(), 1, "Clients accessor works");
	check_size_eq(stats->conns_new, 1, "Connections set");

	stats_flush();

	check_size_eq(stats->clients, 1, "Clients not reset");
	check_size_eq(stats_clients(), 1, "Clients accessor works");
	check_size_eq(stats->conns_new, 0, "Connections reset");
}
END_TEST

static gpointer _stats_tick(gpointer nothing)
{
	usleep(MS_TO_USEC(100));

	stats_flush();

	return NULL;
}

START_TEST(test_stats_sane_tick_graphite)
{
	STATS_INC(clients);
	STATS_INC(conns_new);

	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	gchar port[6];
	snprintf(port, sizeof(port), "%d", option_stats_graphite_port());

	check(getaddrinfo(option_stats_graphite_address(), port, &hints, &res) == 0);
	int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	check(sock != -1);

	int on = 1;
	check(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != -1);

	check(bind(sock, res->ai_addr, res->ai_addrlen) != -1);
	check(listen(sock, 100) != -1);

	freeaddrinfo(res);

	check(stats_init());

	// Put the tick into a new thread so that the recvfrom will get something
	g_thread_new("stats_ticker", _stats_tick, NULL);

	int client = accept(sock, NULL, NULL);

	gchar buff[8192];
	memset(&buff, 0, sizeof(buff));
	read(client, buff, sizeof(buff)-1);

	gchar **messages = g_strsplit(buff, "\n", -1);

	guint64 field_count = 7 * option_apps_count(); // For the stats in app/test
	#define X(slot, name) field_count += 2;
		STATS_S_COUNTERS
	#undef X
	#define X(slot, name) field_count++;
		STATS_S_VALUES
	#undef X

	guint messages_len = g_strv_length(messages)-1 ;

	// Empty string as the last result, but still valid
	check_uint64_eq(messages_len, field_count, "Got all the fields");

	// BAHAHAHAHAHA, use regex to match lines.  OHHHH man, this is awesome
	GRegex* pattern = g_regex_new("qio\\..* \\d+\\.\\d* \\d+",
			 G_REGEX_OPTIMIZE, 0, NULL);

	for (uint i = 0; i < messages_len; i++) {
		check(g_regex_match(pattern, *(messages + i), 0, NULL), "Matches");
	}

	g_regex_unref(pattern);
	g_strfreev(messages);
}
END_TEST

START_TEST(test_stats_sane_tick_no_graphite)
{
	FILE *f = fopen(CONFIG_FILE, "w");
	fwrite(CONFIG_NO_GRAPHITE, 1, sizeof(CONFIG_NO_GRAPHITE), f);
	fclose(f);

	char *argv[] = {"./server", "--config-file="CONFIG_FILE};
	int argc = G_N_ELEMENTS(argv);

	check(option_parse_args(argc, argv, NULL), "File ready");
	check(option_parse_config_file(NULL, NULL, 0, NULL), "Config loaded");

	check(stats_init());
	check(_graphite == NULL);

	STATS_INC(conns_new);

	check_size_eq(stats->conns_new, 1, "Clients right");

	stats_flush();

	check_size_eq(stats->conns_new, 1, "Clients not reset");
}
END_TEST

Suite* stats_suite()
{
	TCase *tc;
	Suite *s = suite_create("Stats");

	tc = tcase_create("Sanity");
	tcase_add_checked_fixture(tc, _stats_setup, NULL);
	tcase_add_test(tc, test_stats_sane_tick);
	tcase_add_test(tc, test_stats_sane_tick_graphite);
	suite_add_tcase(s, tc);

	tc = tcase_create("Sanity (no fixtures)");
	tcase_add_test(tc, test_stats_sane_tick_no_graphite);
	suite_add_tcase(s, tc);

	return s;
}
