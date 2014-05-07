/**
 * Every configuration option that QIO has.
 * @file quick.io/src/config.h
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
 * Address to bind to for plaintext connections.
 */
gchar *cfg_bind_address;

/**
 * The port to bind to for plaintext connections.
 */
guint64 cfg_bind_port;

/**
 * Address to bind to for SSL connections.
 */
gchar *cfg_bind_address_ssl;

/**
 * The port to bind to for SSL connections.
 */
guint64 cfg_bind_port_ssl;

/**
 * The path to create a unix socket at for listening for connections locally.
 */
gchar *cfg_bind_path;

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
 * The first certificate to use for the SSL connection. This MUST be a full
 * certificate chain in PEM format, starting from _your_ certificate on top,
 * all intermediaries, then the root CA.
 *
 * @note Typically RSA.
 */
gchar *cfg_ssl_cert_path_0;

/**
 * The key that corresponds to certificate 0.
 */
gchar *cfg_ssl_key_path_0;

/**
 * Same thing as cfg_ssl_cert_path_0, but for an ECDSA key.
 */
gchar *cfg_ssl_cert_path_1;

/**
 * The key that corresponds to certificate 1.
 */
gchar *cfg_ssl_key_path_1;

/**
 * The initial size for subscriptions.
 */
guint64 cfg_sub_min_size;

/**
 * If the server should listen on port
 */
gboolean cfg_support_flash;

/**
 * The user the server should run as
 */
gchar *cfg_user;

/**
 * Registers all configuration options with QEV.
 */
void config_init();
