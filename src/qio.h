#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <gmodule.h>
#include <libgen.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * The number of milliseconds between maintenance ticks
 */
#define MAINTENANCE_TICK 100

/**
 * Defined in events_client.h
 */
typedef struct evs_client_sub_s evs_client_sub_t;

/**
 * Defined in events_server.h
 */
typedef struct event_handler_s event_handler_t;

/**
 * Defined in events_server.h
 */
typedef struct client_cb_s client_cb_t;

/**
 * Defined in client.h
 */
typedef struct client_s client_t;

/**
 * Defined in client.h
 */
typedef guint16 status_t;

#define QEV_CLIENT_T client_t
#define QEV_CLIENT_SLOT qevclient
#define QEV_CLIENT_NEVER_FREE

#include "quick-event/qev.h"
#include "http-parser/http_parser.h"
#include "reconnecting-socket/reconnecting_socket.h"

#include "module.h"

#include "../test/utils_stats.h"

// Some out-of-order guys for dependencies
#include "stats.h"
#include "events_server.h"
#include "client.h"
#include "events_client.h"

#include "apps.h"
#include "connections.h"
#include "handler_rfc6455.h"
#include "handler_flash_policy.h"
#include "log.h"
#include "main.h"
#include "monkey.h"
#include "option.h"