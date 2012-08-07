/**
 * Utilities for running test cases.
 * @file test_utils.h
 */

#pragma once
#include "test.h"

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