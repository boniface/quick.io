#include <stdio.h>
#include <execinfo.h>
#include <signal.h>

#include "qio.h"

#define BACKTRACE_SIZE 100

#if defined(COMPILE_DEBUG) || defined(COMPILE_PROFILE)
	static void _sigint_handler(int sig) {
		DEBUG("SIGINT: Dying");
		exit(50);
	}
	
	static void _sigsev_handler(int sig) {
		void *array[BACKTRACE_SIZE];
		size_t size;

		// get void*'s for all entries on the stack
		size = backtrace(array, BACKTRACE_SIZE);
		
		// print out all the frames to stderr
		fprintf(stderr, "Error: segfault\n");
		
		// +1 -> skip this function
		backtrace_symbols_fd(array + 1, size, 2);
		exit(11);
	}
	
	static void _sigterm_handler(int sig) {
		exit(51);
	}
#endif

void debug_handle_signals() {
	#if defined(COMPILE_DEBUG) || defined(COMPILE_PROFILE)
		signal(SIGINT, _sigint_handler);
		signal(SIGSEGV, _sigsev_handler);
		signal(SIGTERM, _sigterm_handler);
	#endif
}