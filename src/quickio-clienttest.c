/**
 * The partner app to quickio-clienttest
 */

#define G_LOG_DOMAIN "test_app_sane"
#define EV_PREFIX "/clienttest"
#include "quickio.h"

static enum evs_status _close_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	qev_close(client, 0);
	return EVS_STATUS_HANDLED;
}

static enum evs_status _ping_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	qev_close(client, 0);
	return EVS_STATUS_HANDLED;
}

static gboolean _app_init()
{
	evs_add_handler(EV_PREFIX, "/close", _close_handler, evs_no_on, NULL, FALSE);
	evs_add_handler(EV_PREFIX, "/ping", _ping_handler, evs_no_on, NULL, FALSE);
	return TRUE;
}

static gboolean _app_exit()
{
	return TRUE;
}

QUICKIO_APP(
	_app_init,
	_app_exit);
