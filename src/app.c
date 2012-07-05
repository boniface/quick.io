#include "app.h"
#include "debug.h"

// The thread that the app is running in.
static GThread *_thread;

gboolean app_init() {
	_thread = g_thread_try_new(__FILE__, app_run, NULL, NULL);
	
	if (_thread == NULL) {
		return FALSE;
	}
	
	return TRUE;
}