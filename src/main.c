#include <libgen.h> // For dirname()
#include <sys/select.h>
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
			exit(1);
		} else {
			// Save the process id for later culling
			*(_pids + processes) = pid;
		}
	}
	
	return TRUE;
}

static void _main_check_children() {
	if (errno == EINTR) {
		int status = 0;
		pid_t pid = waitpid(-1, &status, WNOHANG);
		if (pid != 0 && WIFEXITED(status)) {
			ERRORF("Child pid=%d died with status=%d", pid, WEXITSTATUS(status));
			main_cull_children();
		}
	}
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
	
	qsys_socket_t stats_sock = qsys_listen(option_bind_address(), option_stats_port());
	if (stats_sock == -1) {
		ERROR("Could not listen on stats interface.");
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
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(stats_sock, &rfds);
		
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		int ready = select(stats_sock + 1, &rfds, NULL, NULL, &tv);
		if (ready == -1) {
			_main_check_children();
		} else if (ready) {
			qsys_socket_t client;
			while ((client = accept(stats_sock, NULL, NULL)) != -1) {
				stats_request(client);
				close(client);
			}
		} else {
			stats_tick();
		}
	}
	
	return 1;
}

#ifdef TESTING
#include "../test/test_main.c"
#endif