/**
 * Utilities for running tests.
 * @file test.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include <check.h>
#include "quickio.h"

/**
 * For communicating with QIO, this is a crazy simple little client
 */
typedef struct test_client test_client_t;

/**
 * Create a new test suite, ready for running.
 *
 * @param suite_name
 *     Name of the test suite
 * @param sr
 *     Where the test runner should be put
 * @param s
 *     Where the test suite should be put
 */
void test_new(
	const gchar *suite_name,
	SRunner **sr,
	Suite **s);

/**
 * Runs a test and returns if everything passed.
 *
 * @param sr
 *     The test runner to run and free
 */
gboolean test_do(SRunner *sr);

/**
 * A checked fixture: starts up QIO and gets everything ready
 * for a test
 */
void test_setup();

/**
 * A checked teardown: tears down QIO and frees up all memory used
 */
void test_teardown();

/**
 * Adds all of the test cases to the test suite. By default, qio
 * will be initialized and running before the test cases begins.
 *
 * @param suite
 *     The suite to add the tests to
 * @param name
 *     The name of the test cases
 * @param ...
 *     The test functions to add
 *
 * @return The test case, for adding some special-case tests.
 */
TCase* test_add(Suite *s, const gchar *name, ...) G_GNUC_NULL_TERMINATED;

/**
 * Get a client connected to QIO.
 */
test_client_t* test_client(const gboolean ssl);

/**
 * Send a message from the client to the server
 */
void test_send(
	test_client_t *tclient,
	const gchar *data,
	const guint64 len);

/**
 * Receive a message from the server to the client
 */
guint64 test_recv(
	test_client_t *tclient,
	gchar *data,
	const guint64 len);

/**
 * Close and free up any resources the client was using.
 */
void test_close(test_client_t *tclient);
