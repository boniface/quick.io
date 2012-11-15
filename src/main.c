#define QEV_CLIENT_NEW_FN conns_client_new
#define QEV_CLIENT_CLOSE_FN conns_client_close
#define QEV_CLIENT_READ_FN conns_client_data
#define QEV_TIMERS \
	QEV_TIMER(conns_maintenance_tick, 0, MAINTENANCE_TICK) \
	QEV_TIMER(stats_flush, STATS_INTERVAL, 0) \
	QEV_TIMER(evs_client_heartbeat, 60, 0)

#include "qio.h"

#ifdef TESTING
int init_main(int argc, char *argv[]) {
#else
int main(int argc, char *argv[]) {
#endif
	debug_handle_signals();
	
	if (qev_init() == 0) {
		DEBUG("Quick-event inited");
	} else {
		ERROR("Could not init quick-event");
		return 1;
	}
	
	GError *error = NULL;
	if (option_parse_args(argc, argv, &error)) {
		DEBUG("Options parsed");
	} else {
		ERROR(error != NULL ? error->message : "The arguments are not happy");
		return 1;
	}
	
	// Move into the directory holding this binary
	// Only do so after parsing the args so that the config file path is maintained
	chdir(dirname(argv[0]));
	
	if (option_parse_config_file(NULL, NULL, 0, &error)) {
		DEBUG("Config file parsed");
	} else {
		ERROR(error->message);
		return 1;
	}
	
	if (evs_server_init()) {
		DEBUG("Server events inited");
	} else {
		ERROR("Could not init server events.");
		return 1;
	}
	
	if (evs_client_init()) {
		DEBUG("Client events inited");
	} else {
		ERROR("Could not init client events.");
		return 1;
	}
	
	if (conns_init()) {
		DEBUG("Connections handler inited");
	} else {
		ERROR("Could not init connections handler.");
		return 1;
	}
	
	if (stats_init()) {
		DEBUG("Stats handler inited");
	} else {
		ERROR("Could not init stats handler.");
		return 1;
	}
	
	if (apps_run()) {
		DEBUG("Apps running");
	} else {
		ERROR("Could not run the apps.");
		return 1;
	}
	
	if (option_bind_address() == NULL && option_bind_address_ssl() == NULL) {
		ERROR("Neither bind-address nor bind-address-ssl was specified. Can't run a server without listening for connections.  Exiting.");
		return 1;
	}
	
	if (option_bind_address() != NULL) {
		if (qev_listen(option_bind_address(), option_bind_port()) == 0) {
			DEBUGF("Listening on: %s:%d", option_bind_address(), option_bind_port());
		} else {
			ERRORF("Could not listen on %s:%d", option_bind_address(), option_bind_port());
			return 1;
		}
	}
	
	if (option_bind_address_ssl() != NULL) {
		if (qev_listen_ssl(option_bind_address_ssl(), option_bind_port_ssl(), option_ssl_cert_chain(), option_ssl_private_key()) == 0) {
			DEBUGF("SSL Listening on: %s:%d", option_bind_address_ssl(), option_bind_port_ssl());
		} else {
			ERRORF("SSL could not run on %s:%d", option_bind_address_ssl(), option_bind_port_ssl());
			return 1;
		}
	}
	
	if (option_user() != NULL) {
		if (qev_chuser(option_user()) == 0) {
			DEBUGF("Changed to user %s", option_user());
		} else {
			ERRORF("Could not change to user %s", option_user());
			return 1;
		}
	}
	
	#if defined(TESTING) || defined(COMPILE_DEBUG)
		printf("QIO READY\n");
		fflush(stdout);
	#endif
	
	// The main thread also runs in qev, so start at 1, not 0
	for (gint i = 1; i < option_threads(); i++) {
		char buff[16];
		snprintf(buff, sizeof(buff), "qio_th_%d", i);
		g_thread_new(buff, qev_run_thread, NULL);
	}
	
	qev_run();
	
	return 1;
}

#ifdef TESTING
#include "../test/test_main.c"
#endif

#include "../ext/quick-event/qev.c"