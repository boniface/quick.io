/**
 * Starts up the server.
 * @file main.h
 */
#pragma once

/**
 * @mainpage Automatically generated API documentation.
 *
 * <p>For a tutorial, check out the <a href="..">Sphinx documentation</a>.</p>
 * <p>For app-specific functions, check out the @ref AppFunctions page.</p>
 */

#ifdef TESTING
/**
 * Only available when testing: inits the server.
 */
int init_main(int argc, char *argv[]);
#endif

#ifdef TESTING
#include "../test/test_main.h"
#endif
