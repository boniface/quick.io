/**
 * @attention The event used in this file, /test/.*, is registered in app/test.c (the test
 * app used for unit testing apps.  It's loaded with apps_init() here.
 */

#include "test.h"

#define PUB_EVENT "\x81""\x1d""/test/event:0:plain=something"
#define INVALID_EVENT "\x81""\x21""/qio/callback/1:0:plain=qio_error"

#define CLIENT_SEND "\x81""\x14""/test/event:0:plain="

#define ASYNC_CALLBACK "\x81""\x1d""/qio/callback/654987:0:plain="
#define HEARTBEAT_EVENT "\x81""\x17""/qio/heartbeat:0:plain="
#define HEARTBEAT_EVENT_MASKED "\x81\x97""abcd""N""\x13""\n""\x0b""N""\n""\x06""\x05""\x13""\x16""\x01""\x01""\x00""\x16""Y""T""[""\x12""\x0f""\x05""\x08""\x0c""^"

#define HEARBEAT_CHALLENGE_CB "\x81\x98""abcd""N""\x13""\n""\x0b""N""\x01""\x02""\x08""\r""\x00""\x02""\x07""\n""M""R""^""Q""X""\x13""\x08""\x00""\x0b""\r""Y"

#define HEARTBEAT_CHALLENGE "\x81""\x17""/qio/heartbeat:1:plain="

void _test_evs_client_setup()
{
	qev_init();
	utils_stats_setup();
	option_parse_args(0, NULL, NULL);
	option_parse_config_file(NULL, NULL, 0);
	evs_server_init();
	evs_client_init();
	conns_init();
	apps_run();
	check(stats_init());
}

void _test_evs_client_teardown()
{
	utils_stats_teardown();
}

START_TEST(test_evs_client_subscription_get_no_clients)
{
	struct evs_client_sub *sub = _evs_client_subscription_get("/test/event", FALSE, FALSE);
	check_ptr_eq(sub, NULL, "Not created when no clients");
}
END_TEST

START_TEST(test_evs_client_subscription_get_not_valid)
{
	struct evs_client_sub *sub = _evs_client_subscription_get("/what/is/that", FALSE, FALSE);
	check_ptr_eq(sub, NULL, "Not created when no clients");
}
END_TEST

START_TEST(test_evs_client_subscription_get_and_create)
{
	struct evs_client_sub *sub = _evs_client_subscription_get("/test/event", TRUE, FALSE);

	check_str_eq(sub->event_path, "/test/event", "Not created when no clients");
	check(sub->handler != NULL, "Handler is set");
	check(sub->clients != NULL, "Clients table is set");
	check_uint32_eq(g_hash_table_size(sub->clients), 0, "No clients in table");
	check_uint64_eq(sub->extra->len, 0, "No extra parameters");

	check(g_hash_table_lookup(_events, sub->event_path) != NULL, "Event registered");
}
END_TEST

START_TEST(test_evs_client_subscription_get_and_create_invalid)
{
	struct evs_client_sub *sub = _evs_client_subscription_get("/test/not/an/event", TRUE, FALSE);
	check(sub == NULL, "Bad event, not created");
}
END_TEST

START_TEST(test_evs_client_format_message_0)
{
	struct client *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);

	check_status_eq(evs_client_sub_client("/test/event", client, 0),
				CLIENT_GOOD, "Subscribed");
	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_format_message(handler, 0, 0, NULL,
						d_plain, "some data", buffer);

	check_status_eq(status, CLIENT_GOOD, "Message format succeeded");
	check_str_eq(buffer->str, "/test/event:0:plain=some data",
			"Message formatted correctly");

	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_1)
{
	struct client *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);

	check_status_eq(evs_client_sub_client("/test/event", client, 0),
			CLIENT_GOOD, "Subscribed");
	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_format_message(handler, 0, 0,
						NULL, d_plain, "ßäū€öł", buffer);

	check_status_eq(status, CLIENT_GOOD, "Message format succeeded");
	check_str_eq(buffer->str, "/test/event:0:plain=ßäū€öł",
			"Message formatted correctly");

	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_2)
{
	struct client *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);

	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Subscribed");

	path_extra_t *extra = NULL;
	struct event_handler *handler = evs_server_get_handler("/test/event/test", &extra);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_format_message(handler, 0, 0, extra,
						d_plain, "ßäū€öł", buffer);

	check_status_eq(status, CLIENT_GOOD, "Message format succeeded");
	check_str_eq(buffer->str, "/test/event/test:0:plain=ßäū€öł",
				"Message formatted correctly");

	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_no_handler)
{
	struct client *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);

	check_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD,
				"Subscribed");

	enum status status = evs_client_format_message(NULL, 0, 0, NULL, d_plain,
					"ßäū€öł", buffer);

	check_status_eq(status, CLIENT_ERROR, "Could not find handler");
	check_size_eq(buffer->len, 0, "No message set");

	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_invalid_handler)
{
	struct client *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);

	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Subscribed");

	enum status status = evs_client_format_message((const struct event_handler *)1000,
						0, 0, NULL, d_plain,
						"ßäū€öł", buffer);

	check_status_eq(status, CLIENT_ERROR, "Could not find handler");
	check_size_eq(buffer->len, 0, "No message set");

	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_with_callback)
{
	struct client *client = u_client_create(NULL);
	GString *buffer = g_string_sized_new(1);

	check_status_eq(evs_client_sub_client("/test/event/test", client, 0), CLIENT_GOOD,
					"Subscribed");

	path_extra_t *extra = NULL;
	struct event_handler *handler = evs_server_get_handler("/test/event/test", &extra);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_format_message(NULL, 1, 0, extra,
							d_plain, "ßäū€öł", buffer);

	check_status_eq(status, CLIENT_GOOD, "Got message");
	check_str_eq(buffer->str, "/qio/callback/1:0:plain=ßäū€öł",
			"Message formatted correctly");

	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_format_message_with_server_callback)
{
	GString *buffer = g_string_sized_new(1);

	path_extra_t *extra = NULL;
	struct event_handler *handler = evs_server_get_handler("/test/event/test", &extra);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_format_message(NULL, 1, 123, extra,
						d_plain, "ßäū€öł", buffer);

	check_status_eq(status, CLIENT_GOOD, "Got message");
	check_str_eq(buffer->str, "/qio/callback/1:123:plain=ßäū€öł",
					"Message formatted correctly");

	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
}
END_TEST

START_TEST(test_evs_client_format_message_not_subscribed)
{
	GString *buffer = g_string_sized_new(1);

	path_extra_t *extra = NULL;
	struct event_handler *handler = evs_server_get_handler("/test/event/test", &extra);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_format_message(handler, 0, 0, extra, d_plain, "abcd", buffer);

	check_status_eq(status, CLIENT_GOOD, "Message formatted even though no clients subscribed");
	check_str_eq(buffer->str, "/test/event/test:0:plain=abcd", "Message formatted correctly");

	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
}
END_TEST

START_TEST(test_evs_client_format_message_not_subscribed_callback)
{
	GString *buffer = g_string_sized_new(1);

	path_extra_t *extra = NULL;
	struct event_handler *handler = evs_server_get_handler("/test/event/test", &extra);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_format_message(handler, 123, 0, extra,
						d_plain, "abcd", buffer);

	check_status_eq(status, CLIENT_GOOD, "Got message");
	check_str_eq(buffer->str, "/qio/callback/123:0:plain=abcd",
			"Message formatted correctly");

	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
}
END_TEST

START_TEST(test_evs_client_format_message_null_data)
{
	GString *buffer = g_string_sized_new(1);

	path_extra_t *extra = NULL;
	struct event_handler *handler = evs_server_get_handler("/test/event/test", &extra);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_format_message(handler, 123, 0, extra,
						d_plain, NULL, buffer);

	check_status_eq(status, CLIENT_GOOD, "Got message");
	check_str_eq(buffer->str, "/qio/callback/123:0:plain=",
			"Message formatted correctly");

	status = evs_client_format_message(handler, 123, 0, extra, d_plain, "", buffer);

	check_status_eq(status, CLIENT_GOOD, "Got message");
	check_str_eq(buffer->str, "/qio/callback/123:0:plain=",
			"Message formatted correctly");

	g_ptr_array_unref(extra);
	g_string_free(buffer, TRUE);
}
END_TEST

START_TEST(test_evs_client_ready)
{
	struct client *client = u_client_create(NULL);
	g_ptr_array_free(client->subs, TRUE);
	client->subs = NULL;

	evs_client_client_ready(client);
	check(client->subs != NULL, "Subscriptions setup");
	check_size_eq(client->subs->len, 0, "No subscriptions");

	check_size_eq(utils_stats()->apps_client_subscribe, 0,
			"Only real subscribes sent");
	check_size_eq(utils_stats()->apps_client_unsubscribe, 0,
			"Only real unsubscribes sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_heartbeat)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	conns_client_new(client);
	client->handler = h_rfc6455;
	client->state = cstate_running;

	evs_client_heartbeat();

	char buff[512];
	memset(buff, 0, sizeof(buff));
	check_size_eq(read(socket, buff, sizeof(buff)-1),
			sizeof(HEARTBEAT_EVENT)-1, "Correct len");
	check_bin_eq(buff, HEARTBEAT_EVENT, sizeof(HEARTBEAT_EVENT)-1,
			"Correct event sent");

	check_size_eq(stats->evs_client_heartbeats, 1, "Got 1 heartbeat");

	close(socket);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_heartbeat_no_clients)
{
	// No segfaults?
	evs_client_heartbeat();
}
END_TEST

START_TEST(test_evs_heartbeat_yield)
{
	int socket = 0;

	for (int i = 0; i < CONNS_YIELD*2; i++) {
		struct client *client = u_client_create(&socket);
		conns_client_new(client);
		client->handler = h_rfc6455;
		client->state = cstate_running;
	}

	evs_client_heartbeat();

	char buff[512];
	memset(buff, 0, sizeof(buff));
	check_size_eq(read(socket, buff, sizeof(buff)-1),
			sizeof(HEARTBEAT_EVENT)-1, "Correct len");
	check_bin_eq(buff, HEARTBEAT_EVENT, sizeof(HEARTBEAT_EVENT)-1,
			"Correct event sent");

	check_size_eq(stats->evs_client_heartbeats, CONNS_YIELD*2, "Heartbeats sent");
}
END_TEST

START_TEST(test_evs_heartbeat_already_written)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	conns_client_new(client);
	client->handler = h_rfc6455;
	client->state = cstate_running;

	evs_client_heartbeat();
	check_size_eq(stats->evs_client_heartbeats, 1, "Got heartbeat");

	stats->evs_client_heartbeats = 0;

	client_write_frame(client, "test", 4);

	evs_client_heartbeat();

	check_size_eq(stats->evs_client_heartbeats, 0, "Got no heartbeat");

	close(socket);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_heartbeat_inactive_0)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	conns_client_new(client);
	client->handler = h_rfc6455;
	client->state = cstate_running;

	qev_time = client->last_receive = 1000;
	evs_client_heartbeat();

	check_size_eq(stats->evs_client_heartbeats_inactive_challenges, 0,
			"No challenges sent");

	qev_time += HEARTBEAT_INACTIVE + 1;
	evs_client_heartbeat();

	check_size_eq(stats->evs_client_heartbeats_inactive_challenges, 1,
			"Challenge sent");

	qev_time += HEARTBEAT_INACTIVE;
	evs_client_heartbeat();

	check_size_eq(stats->evs_client_heartbeats_inactive_challenges, 1,
			"Challenge sent");
	check_size_eq(stats->evs_client_heartbeats_inactive_closed, 1,
			"Client closed on failed challenge");

	close(socket);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_heartbeat_inactive_1)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	conns_client_new(client);
	client->handler = h_rfc6455;
	client->state = cstate_running;

	qev_time = client->last_receive = 1000;
	evs_client_heartbeat();

	check_size_eq(stats->evs_client_heartbeats_inactive_challenges, 0,
			"No challenges sent");

	qev_time += HEARTBEAT_INACTIVE + 1;
	evs_client_heartbeat();

	check_size_eq(stats->evs_client_heartbeats_inactive_challenges, 1,
			"Challenge sent");

	g_string_append_len(client->message->socket_buffer, HEARTBEAT_EVENT_MASKED,
					sizeof(HEARTBEAT_EVENT_MASKED)-1);

	check_status_eq(client_message(client), CLIENT_GOOD, "Data good");
	check_int64_eq(client->last_receive, qev_time, "Client time updated");

	evs_client_heartbeat();

	check_size_eq(stats->evs_client_heartbeats_inactive_challenges, 1,
			"Challenge sent");
	check_size_eq(stats->evs_client_heartbeats_inactive_closed, 0,
			"Client not closed");

	close(socket);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_heartbeat_inactive_2)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	conns_client_new(client);
	client->handler = h_rfc6455;
	client->state = cstate_running;

	qev_time = HEARTBEAT_INACTIVE + 1;
	client->last_receive = 0;
	evs_client_heartbeat();

	check_size_eq(stats->evs_client_heartbeats_inactive_challenges, 1,
			"Challenge sent");

	char buff[1024];
	memset(buff, 0, sizeof(buff));
	read(socket, buff, sizeof(buff)-1);

	check_bin_eq(buff, HEARTBEAT_CHALLENGE, sizeof(HEARTBEAT_CHALLENGE)-1,
			"Challenge with callback sent");

	g_string_append_len(client->message->socket_buffer,
				HEARBEAT_CHALLENGE_CB,
				sizeof(HEARBEAT_CHALLENGE_CB)-1);
	check_status_eq(client_message(client), CLIENT_GOOD, "Data good");
	check_int64_eq(client->last_receive, qev_time, "Client time updated");

	close(socket);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_bad_subscription_0)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("test", client, 0),
			CLIENT_ERROR, "Subscription not found");
	check_size_eq(client->subs->len, 0, "No subscriptions");

	check_size_eq(utils_stats()->apps_client_subscribe, 0,
			"Only real subscribes sent");
	check_size_eq(utils_stats()->apps_client_unsubscribe, 0,
			"Only real unsubscribes sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_bad_subscription_1)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client(NULL, client, 0), CLIENT_ERROR,
			"Subscription not found");
	check_size_eq(client->subs->len, 0, "No subscriptions");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_bad_subscription_2)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("", client, 0), CLIENT_ERROR,
			"Subscription not found");
	check_size_eq(client->subs->len, 0, "No subscriptions");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_too_many_subscriptions)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Subscribed");

	guint64 max = option_max_subscriptions();
	while (max--) {
		g_ptr_array_add(client->subs, NULL);
	}

	check_status_eq(evs_client_sub_client("/test/event/test2", client, 0),
			CLIENT_ERROR, "Subscription not found");

	check_size_eq(utils_stats()->apps_client_subscribe,
			1 * option_apps_count(),
			"Only real subscribes sent");
	check_size_eq(utils_stats()->apps_client_unsubscribe,
			0 * option_apps_count(),
			"Only real unsubscribes sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_already_subscribed)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Double subscription ignored");

	struct evs_client_sub *sub = _evs_client_subscription_get("/test/event/test", FALSE, FALSE);
	check_size_eq(client->subs->len, 1, "Only 1 subscription");
	check_ptr_eq(g_ptr_array_index(client->subs, 0), sub, "In subscription");

	check_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(),
			"Only real subscribes sent");
	check_size_eq(utils_stats()->apps_client_unsubscribe, 0,
			"Only real unsubscribes sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_reject_child)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("/test/children/reject", client, 0),
			CLIENT_ERROR, "Rejected");
	check_status_eq(evs_client_sub_client("/test/children/reject/hi", client, 0),
			CLIENT_ERROR, "Rejected");
	check_status_eq(evs_client_sub_client("/test/children/reject/another", client, 0),
			CLIENT_ERROR, "Rejected");

	check_size_eq(client->subs->len, 0, "Only 1 subscription");

	check_size_eq(utils_stats()->apps_client_subscribe, 0, "No subscriptions");
	check_size_eq(utils_stats()->apps_client_unsubscribe, 0, "No unsubscriptions");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_async)
{
	struct client *client = u_client_create(NULL);
	check_status_eq(evs_client_sub_client("/test/async", client, 0), CLIENT_ASYNC, "Async");
	check_size_eq(client->subs->len, 0, "Not subscribed, gone ASYNC");

	while (client->subs->len != 1);

	check_size_eq(client->subs->len, 1, "Only 1 subscription");

	check_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "1 sub");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_async_with_callback)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	client->handler = h_rfc6455;

	check_status_eq(evs_client_sub_client("/test/async", client, 654987), CLIENT_ASYNC, "Async");
	check_size_eq(client->subs->len, 0, "Not subscribed, gone ASYNC");

	while (client->subs->len != 1);

	check_size_eq(client->subs->len, 1, "Only 1 subscription");
	check_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "1 sub");

	char buff[512];
	memset(buff, 0, sizeof(buff));
	check_size_eq(read(socket, buff, sizeof(buff)-1), sizeof(ASYNC_CALLBACK)-1, "Correct len");
	check_bin_eq(buff, ASYNC_CALLBACK, sizeof(ASYNC_CALLBACK)-1, "Correct callback sent");
}
END_TEST

START_TEST(test_evs_client_subscribe_async_without_callback)
{
	struct client *client = u_client_create(NULL);
	client->handler = h_rfc6455;

	check_status_eq(evs_client_sub_client("/test/async", client, 0), CLIENT_ASYNC, "Async");
	check_size_eq(client->subs->len, 0, "Not subscribed, gone ASYNC");

	while (client->subs->len != 1);

	check_size_eq(client->subs->len, 1, "Only 1 subscription");

	check_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(), "1 sub");
}
END_TEST

START_TEST(test_evs_client_subscribe_async_reject)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	client->handler = h_rfc6455;

	check_status_eq(evs_client_sub_client("/test/async/reject", client, 1),
			CLIENT_ASYNC, "Async");
	check_size_eq(client->subs->len, 0, "Not subscribed, gone ASYNC");

	check_size_eq(client->subs->len, 0, "Subscription rejected");

	check_size_eq(utils_stats()->apps_client_subscribe, 0 * option_apps_count(), "All rejected");

	gchar buff[512];
	int len = read(socket, buff, sizeof(buff)-1);
	check_int32_eq(len, sizeof(INVALID_EVENT)-1, "Correct message size sent");
	check_bin_eq(buff, INVALID_EVENT, sizeof(INVALID_EVENT)-1, "Correct message sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_async_reject_too_many_subscriptions)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	client->handler = h_rfc6455;

	for (guint32 i = 0; i < option_max_subscriptions(); i++) {
		char ep[128];
		snprintf(ep, sizeof(ep), "/test/async/%d", i);
		check_status_eq(evs_client_sub_client(ep, client, 0), CLIENT_ASYNC, "Async");
	}

	struct evs_client_sub *sub = _evs_client_subscription_get("/test/async", TRUE, FALSE);
	apps_evs_client_check_subscribe(client, sub->handler, sub->extra, 1);
	g_rw_lock_writer_unlock(&sub->lock);

	gchar buff[512];
	int len = read(socket, buff, sizeof(buff)-1);
	check_int32_eq(len, sizeof(INVALID_EVENT)-1, "Correct message size sent");
	check_bin_eq(buff, INVALID_EVENT, sizeof(INVALID_EVENT)-1, "Correct message sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_stupid_path_0)
{
	struct client *client = u_client_create(NULL);
	client->handler = h_rfc6455;

	check_status_eq(evs_client_sub_client("/", client, 0), CLIENT_ERROR, "Rejected");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscribe_stupid_path_1)
{
	struct client *client = u_client_create(NULL);
	client->handler = h_rfc6455;

	check_status_eq(evs_client_sub_client("", client, 0), CLIENT_ERROR, "Rejected");

	u_client_free(client);
}
END_TEST


START_TEST(test_evs_client_subscribe_stupid_path_2)
{
	struct client *client = u_client_create(NULL);
	client->handler = h_rfc6455;

	check_status_eq(evs_client_sub_client(NULL, client, 0), CLIENT_ERROR, "Rejected");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_unsubscribe)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_unsub_client("/test/event/test", client),
			CLIENT_GOOD, "Unsubscribed");

	check_size_eq(client->subs->len, 0, "No subscriptions");
	check_size_eq(utils_stats()->apps_client_subscribe, 1 * option_apps_count(),
			"Only real subscribes sent");
	check_size_eq(utils_stats()->apps_client_unsubscribe, 1 * option_apps_count(),
			"Only real unsubscribes sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_unsubscribe_bad_subscription)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_unsub_client("/test/event/test", client),
			CLIENT_ERROR, "Subscribed");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_unsubscribe_not_subscribed)
{
	struct client *client = u_client_create(NULL);

	struct evs_client_sub *sub = _evs_client_subscription_get("/test/event/test", TRUE, FALSE);
	check(sub != NULL, "Subscription created");
	g_rw_lock_writer_unlock(&sub->lock);

	check_status_eq(evs_client_unsub_client("/test/event/test", client),
			CLIENT_ERROR, "Subscribed");

	check_size_eq(utils_stats()->apps_client_subscribe, 0, "No subscribes sent");
	check_size_eq(utils_stats()->apps_client_unsubscribe, 0, "No unsubscribes sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_subscription_cleanup_0)
{
	// Just make sure the test completes without segfaulting
	_evs_client_subscription_cleanup("/not/gonna/happen");
}
END_TEST

START_TEST(test_evs_client_subscription_cleanup_1)
{
	// Just make sure the test completes without segfaulting
	struct client *client = u_client_create(NULL);
	struct evs_client_sub *sub = _evs_client_subscription_get("/test/event/test", TRUE, FALSE);
	g_hash_table_add(sub->clients, client);

	g_rw_lock_writer_unlock(&sub->lock);

	_evs_client_subscription_cleanup("/test/event/test");
}
END_TEST

START_TEST(test_evs_client_sub_unsub_0)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_unsub_client("/test/event/test", client),
			CLIENT_GOOD, "Unsubscribed");
	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_unsub_client("/test/event/test", client),
			CLIENT_GOOD, "Unsubscribed");
	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_unsub_client("/test/event/test", client),
			CLIENT_GOOD, "Unsubscribed");

	check_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(),
			"Only real subscribes sent");
	check_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(),
			"Only real unsubscribes sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_sub_unsub_1)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_unsub_client("/test/event/test", client),
			CLIENT_GOOD, "Unsubscribed");
	check_status_eq(evs_client_sub_client("/test/event/test2", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_unsub_client("/test/event/test2", client),
			CLIENT_GOOD, "Unsubscribed");
	check_status_eq(evs_client_sub_client("/test/event/test3", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_unsub_client("/test/event/test3", client),
			CLIENT_GOOD, "Unsubscribed");

	check_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(),
			"Only real subscribes sent");
	check_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(),
			"Only real unsubscribes sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_sub_unsub_2)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("/test/event/test", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_sub_client("/test/event/test2", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_sub_client("/test/event/test3", client, 0),
			CLIENT_GOOD, "Subscribed");

	check_status_eq(evs_client_unsub_client("/test/event/test3", client),
			CLIENT_GOOD, "Unsubscribed");
	check_status_eq(evs_client_unsub_client("/test/event/test2", client),
			CLIENT_GOOD, "Unsubscribed");
	check_status_eq(evs_client_unsub_client("/test/event/test", client),
			CLIENT_GOOD, "Unsubscribed");

	check_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(),
			"Only real subscribes sent");
	check_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(),
			"Only real unsubscribes sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_clean)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("/test/event/test1", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_sub_client("/test/event/test2", client, 0),
			CLIENT_GOOD, "Subscribed");
	check_status_eq(evs_client_sub_client("/test/event/test3", client, 0),
			CLIENT_GOOD, "Subscribed");

	evs_client_client_close(client);

	check_ptr_eq(client->subs, NULL, "No subscriptions");

	check_ptr_eq(_evs_client_subscription_get("/test/event/test1", FALSE, FALSE), NULL,
			"No more clients subscribed 1");
	check_ptr_eq(_evs_client_subscription_get("/test/event/test2", FALSE, FALSE), NULL,
			"No more clients subscribed 2");
	check_ptr_eq(_evs_client_subscription_get("/test/event/test3", FALSE, FALSE), NULL,
			"No more clients subscribed 3");

	check_size_eq(utils_stats()->apps_client_subscribe, 3 * option_apps_count(),
			"Only real subscribes sent");
	check_size_eq(utils_stats()->apps_client_unsubscribe, 3 * option_apps_count(),
			"Unsubscribes sent to client close");

	// So that u_client_free doesn't freak out
	client->subs = g_ptr_array_new();
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_clean_bad_client)
{
	struct client *client = u_client_create(NULL);

	// Nothing to test, just make sure it doesn't segfault
	g_ptr_array_free(client->subs, TRUE);
	client->subs = NULL;

	evs_client_client_close(client);

	// So that u_client_free doesn't freak out
	client->subs = g_ptr_array_new();
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_tick_empty)
{
	// Mainly for coverage
	evs_client_tick();
}
END_TEST

START_TEST(test_evs_client_pub_message)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	client->handler = h_rfc6455;

	check_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_pub_event(handler, NULL, d_plain, "something");
	check_status_eq(status, CLIENT_GOOD, "Message published");

	evs_client_tick();

	gchar buff[1000];
	int len = read(socket, buff, sizeof(buff)-1);
	check_int32_eq(len, sizeof(PUB_EVENT)-1, "Correct message size sent");
	check_bin_eq(buff, PUB_EVENT, sizeof(PUB_EVENT)-1, "Correct message sent");

	close(socket);
	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_tick_closed_client_broadcast)
{
	// Client created without a socket == closed
	struct client *client = u_client_create(NULL);
	client->handler = h_rfc6455;

	check_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check(handler != NULL, "Got handler");

	check_status_eq(evs_client_pub_event(handler, NULL, d_plain, "something"),
			CLIENT_GOOD, "Message published");
	check_status_eq(evs_client_pub_event(handler, NULL, d_plain, "something1"),
			CLIENT_GOOD, "Message published");
	check_status_eq(evs_client_pub_event(handler, NULL, d_plain, "something2"),
			CLIENT_GOOD, "Message published");
	check_status_eq(evs_client_pub_event(handler, NULL, d_plain, "something3"),
			CLIENT_GOOD, "Message published");

	evs_client_tick();
	qev_debug_flush();

	check_ptr_eq(_evs_client_subscription_get("/test/event", FALSE, FALSE), NULL,
			"Client closed, sub freed");
	check_size_eq(stats->evs_client_messages_broadcasted_client_fatal, 4,
			"Client closed in each pub");
}
END_TEST

START_TEST(test_evs_client_tick_closed_client_single)
{
	// Client created without a socket == closed
	struct client *client = u_client_create(NULL);
	client->handler = h_rfc6455;

	evs_client_send_callback(client, 1, 0, d_plain, "test");
	evs_client_send_callback(client, 1, 0, d_plain, "test1");
	evs_client_send_callback(client, 1, 0, d_plain, "test2");
	evs_client_send_callback(client, 1, 0, d_plain, "test3");
	evs_client_send_callback(client, 1, 0, d_plain, "test4");
	evs_client_send_callback(client, 1, 0, d_plain, "test5");

	evs_client_tick();
	check_size_eq(stats->evs_client_messages_callbacks_failure, 6, "Client closed");
}
END_TEST

START_TEST(test_evs_client_tick_no_client_handler)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_pub_event(handler, NULL, d_plain, "something");
	check_status_eq(status, CLIENT_GOOD, "Message published");

	evs_client_tick();
	qev_debug_flush();

	// Client not allocated with a socket, so will be closed
	check_ptr_eq(_evs_client_subscription_get("/test/event", FALSE, FALSE), NULL,
					"Client closed");

	check_size_eq(stats->evs_client_messages_broadcasted, 1, "1 message sent");
	check_size_eq(stats->evs_client_messages_broadcasted_client_fatal, 1, "Client closed");
}
END_TEST

START_TEST(test_evs_client_tick_clear_subscription)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_sub_client("/test/event", client, 0), CLIENT_GOOD, "Subscribed");
	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check(handler != NULL, "Got handler");

	enum status status = evs_client_pub_event(handler, NULL, d_plain, "something");
	check_status_eq(status, CLIENT_GOOD, "Message published");

	conns_client_close(client);

	// Attempt to publish to an empty subscription
	evs_client_tick();

	check_size_eq(stats->evs_client_messages_broadcasted, 0, "No messages sent");
	check_size_eq(stats->evs_client_messages_broadcasted_client_fatal, 0, "Client closed");
}
END_TEST

START_TEST(test_evs_client_tick_no_handler)
{
	enum status status = evs_client_pub_event(NULL, NULL, d_plain, "something");
	check_status_eq(status, CLIENT_ERROR, "Message published");
}
END_TEST

START_TEST(test_evs_client_send_0)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	client->handler = h_rfc6455;
	client->state = cstate_running;

	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check_status_eq(evs_client_send(client, handler, NULL, 0, d_plain, ""),
			CLIENT_GOOD, "Message sent");

	gchar buff[128];
	int len = read(socket, buff, sizeof(buff)-1);
	check_int32_eq(len, sizeof(CLIENT_SEND)-1, "Got right message size");
	check_bin_eq(buff, CLIENT_SEND, sizeof(CLIENT_SEND)-1, "Correct message sent");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_send_1)
{
	struct client *client = u_client_create(NULL);

	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check_status_eq(evs_client_send(client, handler, NULL, 0, d_plain, ""), CLIENT_FATAL,
					"Message rejected");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_send_2)
{
	struct client *client = u_client_create(NULL);

	check_status_eq(evs_client_send(client, NULL, NULL, 0, d_plain, ""),
			CLIENT_FATAL, "Message rejected");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_async_message_invalid_event_0)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);

	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);

	evs_client_app_sub_cb(client, handler, NULL, 0, FALSE);

	evs_client_tick();

	check_size_eq(stats->evs_client_messages_callback, 0, "0 messages sent");
}
END_TEST

START_TEST(test_evs_client_async_message_invalid_event_1)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	client->handler = h_rfc6455;

	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);

	evs_client_app_sub_cb(client, handler, NULL, 1, FALSE);

	evs_client_tick();

	check_size_eq(stats->evs_client_messages_callback, 1, "1 message sent");

	gchar buff[1000];
	int len = read(socket, buff, sizeof(buff)-1);
	check_int32_eq(len, sizeof(INVALID_EVENT)-1, "Message sent");
	check_bin_eq(buff, INVALID_EVENT, sizeof(INVALID_EVENT)-1, "Correct invalid response");
}
END_TEST

START_TEST(test_evs_client_async_message_invalid_event_extra)
{
	int socket = 0;
	struct client *client = u_client_create(&socket);
	client->handler = h_rfc6455;

	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);

	path_extra_t *extra = g_ptr_array_new_with_free_func(g_free);
	g_ptr_array_add(extra, "test");
	g_ptr_array_add(extra, "something");
	evs_client_app_sub_cb(client, handler, extra, 1, FALSE);

	evs_client_tick();

	check_size_eq(stats->evs_client_messages_callback, 1, "1 message sent");

	gchar buff[1000];
	int len = read(socket, buff, sizeof(buff)-1);
	check_int32_eq(len, sizeof(INVALID_EVENT)-1, "Message sent");
	check_bin_eq(buff, INVALID_EVENT, sizeof(INVALID_EVENT)-1, "Correct invalid response");
}
END_TEST

START_TEST(test_evs_client_number_subscribed_0)
{
	struct client *client = u_client_create(NULL);

	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check_uint64_eq(evs_client_number_subscribed(handler, NULL), 0, "No Subscriptions");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_number_subscribed_1)
{
	struct client *client = u_client_create(NULL);

	evs_client_sub_client("/test/event", client, 0);
	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check_uint64_eq(evs_client_number_subscribed(handler, NULL), 1, "Single subscription");

	u_client_free(client);
}
END_TEST

START_TEST(test_evs_client_number_subscribed_2)
{
	struct client *client = u_client_create(NULL);

	path_extra_t *extra = g_ptr_array_new();
	g_ptr_array_add(extra, "something");

	evs_client_sub_client("/test/event/something", client, 0);
	struct event_handler *handler = evs_server_get_handler("/test/event", NULL);
	check_uint64_eq(evs_client_number_subscribed(handler, extra), 1, "Single subscription");

	g_ptr_array_free(extra, TRUE);
	u_client_free(client);
}
END_TEST

Suite* events_client_suite()
{
	TCase *tc;
	Suite *s = suite_create("Events - Client");

	tc = tcase_create("getSubscription");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_subscription_get_no_clients);
	tcase_add_test(tc, test_evs_client_subscription_get_not_valid);
	tcase_add_test(tc, test_evs_client_subscription_get_and_create);
	tcase_add_test(tc, test_evs_client_subscription_get_and_create_invalid);
	suite_add_tcase(s, tc);

	tc = tcase_create("Format Message");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_format_message_0);
	tcase_add_test(tc, test_evs_client_format_message_1);
	tcase_add_test(tc, test_evs_client_format_message_2);
	tcase_add_test(tc, test_evs_client_format_message_no_handler);
	tcase_add_test(tc, test_evs_client_format_message_invalid_handler);
	tcase_add_test(tc, test_evs_client_format_message_with_callback);
	tcase_add_test(tc, test_evs_client_format_message_with_server_callback);
	tcase_add_test(tc, test_evs_client_format_message_not_subscribed);
	tcase_add_test(tc, test_evs_client_format_message_not_subscribed_callback);
	tcase_add_test(tc, test_evs_client_format_message_null_data);
	suite_add_tcase(s, tc);

	tc = tcase_create("Client Ready");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_ready);
	suite_add_tcase(s, tc);

	tc = tcase_create("Heartbeat");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_heartbeat);
	tcase_add_test(tc, test_evs_heartbeat_no_clients);
	tcase_add_test(tc, test_evs_heartbeat_yield);
	tcase_add_test(tc, test_evs_heartbeat_already_written);
	tcase_add_test(tc, test_evs_heartbeat_inactive_0);
	tcase_add_test(tc, test_evs_heartbeat_inactive_1);
	tcase_add_test(tc, test_evs_heartbeat_inactive_2);
	suite_add_tcase(s, tc);

	tc = tcase_create("Subscribe");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_subscribe_bad_subscription_0);
	tcase_add_test(tc, test_evs_client_subscribe_bad_subscription_1);
	tcase_add_test(tc, test_evs_client_subscribe_bad_subscription_2);
	tcase_add_test(tc, test_evs_client_subscribe_too_many_subscriptions);
	tcase_add_test(tc, test_evs_client_subscribe_already_subscribed);
	tcase_add_test(tc, test_evs_client_subscribe_reject_child);
	tcase_add_test(tc, test_evs_client_subscribe_async);
	tcase_add_test(tc, test_evs_client_subscribe_async_with_callback);
	tcase_add_test(tc, test_evs_client_subscribe_async_without_callback);
	tcase_add_test(tc, test_evs_client_subscribe_async_reject);
	tcase_add_test(tc, test_evs_client_subscribe_async_reject_too_many_subscriptions);
	tcase_add_test(tc, test_evs_client_subscribe_stupid_path_0);
	tcase_add_test(tc, test_evs_client_subscribe_stupid_path_1);
	tcase_add_test(tc, test_evs_client_subscribe_stupid_path_2);
	suite_add_tcase(s, tc);

	tc = tcase_create("Unsubscribe");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_unsubscribe);
	tcase_add_test(tc, test_evs_client_unsubscribe_bad_subscription);
	tcase_add_test(tc, test_evs_client_unsubscribe_not_subscribed);
	tcase_add_test(tc, test_evs_client_subscription_cleanup_0);
	tcase_add_test(tc, test_evs_client_subscription_cleanup_1);
	suite_add_tcase(s, tc);

	tc = tcase_create("Subscribe + Unsubscribe");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_sub_unsub_0);
	tcase_add_test(tc, test_evs_client_sub_unsub_1);
	tcase_add_test(tc, test_evs_client_sub_unsub_2);
	suite_add_tcase(s, tc);

	tc = tcase_create("Client Cleaning");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_clean);
	tcase_add_test(tc, test_evs_client_clean_bad_client);
	suite_add_tcase(s, tc);

	tc = tcase_create("Publish Messages");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_tick_empty);
	tcase_add_test(tc, test_evs_client_pub_message);
	tcase_add_test(tc, test_evs_client_tick_closed_client_broadcast);
	tcase_add_test(tc, test_evs_client_tick_closed_client_single);
	tcase_add_test(tc, test_evs_client_tick_no_client_handler);
	tcase_add_test(tc, test_evs_client_tick_clear_subscription);
	tcase_add_test(tc, test_evs_client_tick_no_handler);
	suite_add_tcase(s, tc);

	tc = tcase_create("Publish Messages");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_send_0);
	tcase_add_test(tc, test_evs_client_send_1);
	tcase_add_test(tc, test_evs_client_send_2);
	suite_add_tcase(s, tc);

	tc = tcase_create("Async Message");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_async_message_invalid_event_0);
	tcase_add_test(tc, test_evs_client_async_message_invalid_event_1);
	tcase_add_test(tc, test_evs_client_async_message_invalid_event_extra);
	suite_add_tcase(s, tc);

	tc = tcase_create("Subscribed Count");
	tcase_add_checked_fixture(tc, _test_evs_client_setup, _test_evs_client_teardown);
	tcase_add_test(tc, test_evs_client_number_subscribed_0);
	tcase_add_test(tc, test_evs_client_number_subscribed_1);
	tcase_add_test(tc, test_evs_client_number_subscribed_2);
	suite_add_tcase(s, tc);

	return s;
}
