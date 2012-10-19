#include <libgen.h> // For dirname()
#include <sys/wait.h> // For wait()

#include "qio.h"

/**
 * Allow access to the pids from the test framework
 */
static pid_t *_pids;

/**
 * Fork all of the children processes.
 */
static gboolean _main_fork() {
	gint32 processes = option_processes();
	
	// If this malloc fails, shit is fucked
	_pids = g_malloc0(processes * sizeof(*_pids));
	
	while (processes-- > 0) {
		// Start the child! Hooray!
		pid_t pid = fork();
		
		// If the fork failed, just exit.
		if (pid == -1) {
			ERRORF("Forking children failed at fork #%d", processes);
			return FALSE;
		}
		
		// The child is pid == 0
		if (pid == 0) {
			// Done with these guys
			free(_pids);
			
			option_set_process(processes);
			
			if (apps_run()) {
				DEBUG("Apps running");
			} else {
				ERROR("Could not run the apps.");
				return 1;
			}
			
			qsys_main_loop();
			ERRORF("A CHILD EXITED THE EVENT LOOP: #%d", processes);
			exit(33);
		} else {
			// Save the process id for later culling
			*(_pids + processes) = pid;
		}
	}
	
	return TRUE;
}

static void _sigterm_handler(int sig) {
	WARN("SIGTERM: Killing the children");
	main_cull_children();
	exit(51);
}

void main_cull_children() {
	DEBUG("All children are being culled...");

	gint32 count = option_processes();
	while (count-- > 0) {
		DEBUGF("Killing: %d", count);
		kill(*(_pids + count), SIGINT);
	}
}

#ifdef TESTING
int init_main(int argc, char *argv[]) {
#else
int main(int argc, char *argv[]) {
#endif
	signal(SIGTERM, _sigterm_handler);
	
	debug_handle_signals();
	
	GError *error = NULL;
	if (option_parse_args(argc, argv, &error)) {
		DEBUG("Options parsed");
	} else {
		ERROR(error->message);
		return 1;
	}
	
	// Move into the directory holding this binary
	// Only do so after parsing the args so that the config file path is maintained
	chdir(dirname(argv[0]));
	
	if (option_parse_config_file(NULL, NULL, 0, &error)) {
		DEBUG("Config file parsed");
	} else {
		ERROR(error->message);
		return 1;
	}
	
	if (evs_client_init()) {
		DEBUG("Client events inited");
	} else {
		ERROR("Could not init client events.");
		return 1;
	}
	
	if (evs_server_init()) {
		DEBUG("Server events inited");
	} else {
		ERROR("Could not init server events.");
		return 1;
	}
	
	if (conns_init()) {
		DEBUG("Connections handler inited");
	} else {
		ERROR("Could not init connections handler.");
		return 1;
	}
	
	if (stats_init()) {
		DEBUG("Stats handler inited");
	} else {
		ERROR("Could not init stats handler.");
		return 1;
	}
	
	if (_main_fork()) {
		DEBUG("Children forked");
	} else {
		ERROR("Could not fork children.");
		return 1;
	}
	
	printf("QIO READY\n");
	fflush(stdout);
	
	// The main thread of the parent process manages all of the stats, the stats web interface,
	// and child process monitoring. The child processes will never get here.
	
	while (TRUE) {
		int status = 0;
		pid_t pid = wait(&status);
		if (pid != 0 && WIFEXITED(status)) {
			ERRORF("Child pid=%d died with status=%d", pid, WEXITSTATUS(status));
			main_cull_children();
			exit(2);
		}
	}
	
	return 1;
}

#ifdef TESTING
#include "../test/test_main.c"
#endif