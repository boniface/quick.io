#pragma once

#include <errno.h>
#include <glib.h>
#include <gmodule.h>
#include <libsoup/soup.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STRUCT_PACKED __attribute__((__packed__))

#include "module.h"

// Some out-of-order guys for dependencies
#include "client.h"
#include "events_server.h"

#include "apps.h"
#include "debug.h"
#include "events_client.h"
#include "handler_rfc6455.h"
#include "main.h"
#include "option.h"
#include "socket.h"
#include "validate.h"