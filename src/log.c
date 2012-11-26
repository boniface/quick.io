#include <stdio.h>
#include <execinfo.h>
#include <signal.h>

#include "qio.h"

#define BACKTRACE_SIZE 100
#define DEBUGGING defined(COMPILE_DEBUG) || defined(COMPILE_PROFILE) || defined(COMPILE_PROFILE)

/**
 * The log file that we're currently writing to
 */
static FILE *_log_file = NULL;

#if DEBUGGING
	static void _sigint_handler(int sig) {
		DEBUG("SIGINT: Dying");
		exit(50);
	}
	
	static void _sigsev_handler(int sig) {
		void *array[BACKTRACE_SIZE];
		size_t size;

		// Get void*'s for all entries on the stack
		size = backtrace(array, BACKTRACE_SIZE);
		
		fprintf(_log_file, "Error: segfault\n");
		
		// +1 -> skip this function
		backtrace_symbols_fd(array + 1, size, 2);
		exit(11);
	}
	
	static void _sigterm_handler(int sig) {
		exit(51);
	}
#endif

static gboolean _reload_log_file() {
	#if DEBUGGING
		_log_file = stderr;
	#else
		if (_log_file != NULL && _log_file != stderr) {
			fclose(_log_file);
		}
		
		_log_file = fopen(option_log_file(), "a");
		if (_log_file == NULL) {
			// Make sure some error message gets out of the server
			_log_file = stderr;
			return FALSE;
		}
		
		setvbuf(_log_file, (char *) NULL, _IOLBF, 0);
	#endif
	
	return TRUE;
}

static void _sigusr1_handler(int sig) {
	_reload_log_file();
}

static void _log(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data) {
	fprintf(_log_file, "%s\n", message);
}

gboolean log_init() {
	#if defined(COMPILE_DEBUG) || defined(COMPILE_PROFILE)
		signal(SIGINT, _sigint_handler);
		signal(SIGSEGV, _sigsev_handler);
		signal(SIGTERM, _sigterm_handler);
	#endif
	
	g_log_set_default_handler(_log, NULL);
	
	if (!_reload_log_file()) {
		ERROR("Could not open log file");
		return FALSE;
	}
	
	signal(SIGUSR1, _sigusr1_handler);
	
	return TRUE;
}