/**
 * Every configuration option that QIO has.
 * @file
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#pragma once
#include "quickio.h"

/**
 * Number of threads used to pump out each broadcasted message. These threads
 * come from the qev_pool of threads.
 */
guint64 cfg_broadcast_threads;

/**
 * How long a callback should be allowed to live on the server before
 * being killed.
 */
guint64 cfg_clients_cb_max_age;

/**
 * The total number of event subscriptions that may exist on a server.
 */
guint64 cfg_clients_subs_total;

/**
 * After what percent of `cfg_clients_subs_total` memory pressure should be
 * applied.
 */
guint64 cfg_clients_subs_pressure;

/**
 * How many subscriptions each client is entitled to, even when under
 * pressure.
 */
guint64 cfg_clients_subs_min;

/**
 * Number of threads to use to run periodic tasks
 */
guint64 cfg_periodic_threads;

/**
 * The hostname of this server on the internet
 */
gchar *cfg_public_address;

/**
 * If the server should run app tests and exit.
 */
gboolean cfg_run_app_tests;

/**
 * The initial size for subscriptions.
 */
guint64 cfg_sub_min_size;

/**
 * Registers all configuration options with QEV.
 */
void config_init();
