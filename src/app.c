#include "app.h"
#include "debug.h"

// The thread that the app is running in.
static GThread *_thread;

void app_init() {
	_thread = g_thread_create(app_run, NULL, TRUE, NULL);
}