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
 * Get a new socket connected to localhost on the test's port
 */
qev_fd_t test_socket();

/**
 * Get a client connected to QIO.
 */
qev_fd_t test_client();

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
	qev_fd_t tc,
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
	qev_fd_t tc,
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
	qev_fd_t tc,
	gchar *data,
	const guint64 len);

/**
 * Tests that the received message is what is given.
 */
void test_msg(qev_fd_t tc, const gchar *data);

/**
 * Tests that the given callback is recieved in response to the message
 */
void test_cb(qev_fd_t tc, const gchar *msg, const gchar *data);

/**
 * Tests that the client is alive by pinging the server
 */
void test_ping(qev_fd_t tc);

/**
 * Waits for all of the connected clients to have userspace buffers.
 * Typically, this is only useful when testing 1 connected client.
 *
 * @param len
 *     The MINIMUM length that the buffer must be before returning.
 */
void test_wait_for_buff(const guint len);

/**
 * Tests that the client was closed by the server.
 */
void test_client_dead(qev_fd_t tc);

/**
 * Close and free up any resources the client was using.
 */
void test_close(qev_fd_t tc);
