#ifndef QIO_QIO_H
#define QIO_QIO_H

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
 * Defined in events_server.h
 */
struct event_handler;

/**
 * Defined in client.h
 */
struct client;

/**
 * Defined in client.h
 */
enum status;

#define QEV_CLIENT_T struct client
#define QEV_CLIENT_SLOT qevclient
#define QEV_CLIENT_NEVER_FREE

#include "lib/quick-event/qev.h"
#include "lib/http_parser.h"
#include "lib/reconnecting_socket.h"

#include "module.h"

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

#include "test/utils_stats.h"

#endif
