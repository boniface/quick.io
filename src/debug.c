#include <stdio.h>
#include <execinfo.h>
#include <signal.h>

#include "qio.h"

#define BACKTRACE_SIZE 100

#ifdef COMPILE_DEBUG
	static void _sigsev_handler(int sig) {
		void *array[BACKTRACE_SIZE];
		size_t size;

		// get void*'s for all entries on the stack
		size = backtrace(array, BACKTRACE_SIZE);
		
		// print out all the frames to stderr
		fprintf(stderr, "Error: signal %d:\n", sig);
		backtrace_symbols_fd(array, size, 2);
		exit(1);
	}
	
	static void _sigint_handler(int sig) {
		DEBUG("Sigint: Dying");
		exit(1);
	}
#endif

void debug_handle_signals() {
	#ifdef COMPILE_DEBUG
		signal(SIGSEGV, _sigsev_handler);
		signal(SIGINT, _sigint_handler);
	#endif
}