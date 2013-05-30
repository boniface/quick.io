#include <stdio.h>
#include <execinfo.h>
#include <signal.h>

#include "qio.h"

#define BACKTRACE_SIZE 100
#define DEBUGGING defined(COMPILE_DEBUG) || defined(TESTING)

/**
 * The log file that we're currently writing to
 */
static FILE *_log_file = NULL;

#if DEBUGGING || PROFILING
	static void _sigint_handler(int sig)
	{
		DEBUG("SIGINT: Dying");
		exit(50);
	}

	static void _sigsev_handler(int sig)
	{
		void *array[BACKTRACE_SIZE];
		size_t size;

		// Get void*'s for all entries on the stack
		size = backtrace(array, BACKTRACE_SIZE);

		fprintf(stderr, "Error: segfault\n");

		// +1 -> skip this function on the stack
		backtrace_symbols_fd(array + 1, size, 2);
		exit(11);
	}

	static void _sigterm_handler(int sig)
	{
		exit(51);
	}
#endif

static gboolean _reload_log_file()
{
	if (option_log_file() == NULL) {
		_log_file = stderr;
	} else {
		if (_log_file != NULL && _log_file != stderr) {
			fclose(_log_file);
		}

		_log_file = fopen(option_log_file(), "a");
		if (_log_file == NULL) {
			// Make sure some error message gets out of the server
			_log_file = stderr;
			return FALSE;
		}

		setvbuf(_log_file, (gchar*)NULL, _IOLBF, 0);
	}

	return TRUE;
}

static void _sigusr1_handler(int sig)
{
	_reload_log_file();
}

static gchar* _get_level(GLogLevelFlags log_level)
{
	switch (log_level & G_LOG_LEVEL_MASK) {
		case G_LOG_LEVEL_ERROR: return "FATAL";
		case G_LOG_LEVEL_CRITICAL: return "CRITICAL";
		case G_LOG_LEVEL_WARNING: return "WARNING";
		case G_LOG_LEVEL_MESSAGE: return "MESSAGE";
		case G_LOG_LEVEL_INFO: return "INFO";
		case G_LOG_LEVEL_DEBUG: return "DEBUG";
		default: return "LOG";
	}
}

static void _log(
	const gchar *log_domain,
	GLogLevelFlags log_level,
	const gchar *message,
	gpointer user_data)
{
	if (log_domain == NULL) {
		log_domain = "QIO";
	}

	#ifdef TESTING
		__gcov_flush();
	#endif

	#if DEBUGGING
		fprintf(_log_file, "%s - %s : %s\n", log_domain,
				_get_level(log_level), message);
	#else
		fprintf(_log_file, "%s - %s [%" G_GINT64_FORMAT "] %s\n",
				log_domain, _get_level(log_level),
				g_get_real_time(), message);
	#endif
}

gboolean log_init()
{
	// OpenSSL sends SIGPIPE which kills the server == bad
	signal(SIGPIPE, SIG_IGN);

	#if DEBUGGING || PROFILING
		signal(SIGINT, _sigint_handler);
		signal(SIGSEGV, _sigsev_handler);
		signal(SIGTERM, _sigterm_handler);
	#endif

	g_log_set_default_handler(_log, NULL);

	if (!_reload_log_file()) {
		CRITICAL("Could not open log file");
		return FALSE;
	}

	signal(SIGUSR1, _sigusr1_handler);

	return TRUE;
}
