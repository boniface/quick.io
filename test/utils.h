/**
 * Utilities for running test cases.
 * @file utils.h
 */

#pragma once
#include "test.h"

/**
 * Convert a status_t to a string, for printing.
 *
 * @param status The status to look for.
 *
 * @return A printable string that names the status, or NULL if not found / multiple statuses.
 */
char* test_status_to_str(status_t status);

/**
 * Create a new client for tests.
 *
 * @return A client that can be used.
 */
client_t* u_client_create();

/**
 * Free the client allocated in u_client_create().
 *
 * @param client The client to free.
 */
void u_client_free(client_t *client);

/**
 * Setup the server and get it running.  Returns once the server is ready to function.
 *
 * @param[out] pid The pid of the running server.
 */
void u_main_setup(pid_t *pid);

/**
 * Kill the server and all its children.
 *
 * @param pid The pid of the server to murder.
 */
void u_main_teardown(pid_t pid);

/**
 * Connect to a running server instance.
 *
 * @return The socket connected to the server.
 */
int u_connect();