/**
 * Starts up the server.
 * @file main.h
 */
#pragma once

/**
 * Cull all of the children forked by main.
 */
void main_cull_children();

#ifdef TESTING
/**
 * Only available when testing: inits the server.
 */
int init_main(int argc, char *argv[]);
#endif

#ifdef TESTING
#include "../test/test_main.h"
#endif