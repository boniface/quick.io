#pragma once

#include <errno.h>
#include <glib.h>
#include <gmodule.h>
#include <http_parser.h>
#include <libgen.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STRUCT_PACKED __attribute__((__packed__))

#include "module.h"

/**
 * Defined in events_client.h
 */
typedef struct evs_client_sub_s evs_client_sub_t;

/**
 * Defined in events_server.h
 */
typedef struct event_handler_s event_handler_t;

/**
 * Defined in client.h
 */
typedef struct client_s client_t;

/**
 * Defined in client.h
 */
typedef guint16 status_t;

#include "../test/utils_stats.h"
#include "../test/utils_locks.h"

// Some out-of-order guys for dependencies
#include "qsys.h"
#include "client.h"
#include "events_server.h"
#include "events_client.h"

#include "apps.h"
#include "connections.h"
#include "debug.h"
#include "handler_rfc6455.h"
#include "handler_flash_policy.h"
#include "main.h"
#include "monkey.h"
#include "option.h"