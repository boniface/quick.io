#pragma once
#include <glib.h>

/**
 * In order to run an app, you need to implement the following function
 * inside your application.
 * 
 * Run the loop for your application.  This runs inside a new thread.
 *
 * The given pointer is ALWAYS NULL.
 */
gpointer app_run(gpointer);

/**
 * The following is implemented for you.  It spawns a new thread for
 * your app to run in.
 */
void app_init(void);