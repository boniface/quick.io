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
test_client_t* test_client();

/**
 * Get a client connected to QIO with SSL.
 */
test_client_t* test_client_ssl();

/**
 * Send a message from the client to the server.
 *
 * @param tclient
 *     The test client to write to
 * @param data
 *     The data to frame and send
 * @param len
 *     The length of the data. 0 to strlen() it data to find out.
 */
void test_send(
	test_client_t *tclient,
	const gchar *data);

/**
 * Send a message from the client to the server.
 *
 * @param tclient
 *     The test client to write to
 * @param data
 *     The data to frame and send
 * @param len
 *     The length of the data. 0 to strlen() it data to find out.
 */
void test_send_len(
	test_client_t *tclient,
	const gchar *data,
	const guint64 len);

/**
 * Receive a message from the server to the client.
 *
 * @param tclient
 *     The client to read from.
 * @param data
 *     Where the data should be put. Will be null-terminated.
 * @param len
 *     The length of the data buffer.
 *
 * @return
 *     The number of bytes read.
 */
guint64 test_recv(
	test_client_t *tclient,
	gchar *data,
	const guint64 len);

/**
 * Tests that the received message is what is given.
 */
void test_msg(test_client_t *tclient, const gchar *data);

/**
 * Tests that the given callback is recieved in response to the message
 */
void test_cb(test_client_t *tclient, const gchar *msg, const gchar *data);

/**
 * Close and free up any resources the client was using.
 */
void test_close(test_client_t *tclient);
