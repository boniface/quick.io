#include <libgen.h> // For dirname()
#include <sys/types.h> // For wait()
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
	_pids = g_try_malloc0(processes * sizeof(*_pids));
	if (_pids == NULL) {
		ERROR("Processes malloc() failed. Lol.");
		return FALSE;
	}
	
	while (processes-- > 0) {
		int pipefd[2];
		
		if (pipe(pipefd) == -1) {
			ERRORF("Could not create pipefd on fork #%d", processes);
			return FALSE;
		}
		
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
			
			// Init the apps. If they fail, then why are we running?
			if (!apps_run()) {
				ERRORF("Could not init apps in #%d.", processes);
				exit(1);
			}
			
			qsys_main_loop(processes);
			ERRORF("A CHILD EXITED THE EVENT LOOP: #%d", processes);
			exit(1);
		} else {
			// Save the process id for later culling
			*(_pids + processes) = pid;
		}
	}
	
	return TRUE;
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
	
	// Move into the directory holding this binary
	chdir(dirname(argv[0]));
	
	GError *error = NULL;
	if (option_parse_args(argc, argv, &error)) {
		DEBUG("Options parsed");
	} else {
		ERROR(error->message);
		return 1;
	}
	
	if (option_parse_config_file(NULL, NULL, 0, &error)) {
		DEBUG("Config file parsed");
	} else {
		ERROR(error->message);
		return 1;
	}
	
	if (apps_init()) {
		DEBUG("Apps inited");
	} else {
		ERROR("Could not init apps.");
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
	
	if (apps_prefork()) {
		DEBUG("Apps preforked");
	} else {
		ERROR("Error with preforking apps.");
		return 1;
	}
	
	if (_main_fork()) {
		DEBUG("Children forked");
	} else {
		ERROR("Could not fork children.");
		return 1;
	}
	
	if (apps_postfork()) {
		DEBUG("Postfork apps done");
	} else {
		ERROR("Error with apps postfork");
		main_cull_children();
		return 1;
	}
	
	printf("READY\n");
	fflush(stdout);
	
	// The main thread just sits here, waiting
	// The children processes will never get here
	gint32 count = option_processes();
	while (count-- > 0) {
		int status = 0;
		pid_t pid = wait(&status);
		
		if (WIFEXITED(status)) {
			ERRORF("Child pid=%d died with status=%d", pid, WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			ERRORF("Child pid=%d was killed by signal %d", pid, WTERMSIG(status));
		} else {
			// The process didn't exit, so keep counting
			count++;
		}
	}
	
	return 1;
}

#ifdef TESTING
#include "../test/test_main.c"
#endif