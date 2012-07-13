#include <glib.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "apps.h"
#include "commands.h"
#include "debug.h"
#include "option.h"
#include "pubsub.h"
#include "socket.h"

/**
 * Fork all of the children processes.
 *
 * *pipes will be set to the write pipes into the children.
 */
static gboolean _main_fork(pid_t **pids) {
	gint processes = option_processes();
	
	// If this malloc fails, shit is fucked
	*pids = malloc(processes * sizeof(**pids));
	if (*pids == NULL) {
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
			free(*pids);
			
			// Get epoll and listening going.  If this fails...shit.
			if (!socket_init_process()) {
				ERRORF("Epoll init failed in #%d", processes);
				exit(1);
			}
			
			// Init the apps. If they fail, then why are we running?
			if (!apps_run()) {
				ERRORF("Could not init apps in #%d.", processes);
				exit(1);
			}
			
			// Run the socket loop
			socket_loop();
			ERRORF("A CHILD EXITED THE SOCKET LOOP: #%d", processes);
			exit(1);
		} else {
			// Save the process id for later culling
			*(*pids + processes) = pid;
		}
	}
	
	return TRUE;
}

static void _main_cull_children(int *pids) {
	DEBUG("All children are being culled...");

	int count = option_processes();
	while (count-- > 0) {
		DEBUGF("Killing: %d", count);
		kill(*(pids + count), SIGINT);
	}
}

int main(int argc, char *argv[]) {
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
	
	if (pubsub_init()) {
		DEBUG("Pubsub inited");
	} else {
		ERROR("Could not init pubsub.");
		return 1;
	}
	
	if (commands_init()) {
		DEBUG("Commands inited");
	} else {
		ERROR("Could not init commands.");
		return 1;
	}
	
	if (socket_init()) {
		DEBUG("Socket listener setup");
	} else {
		ERROR("Could not init socket.");
		return 1;
	}
	
	pid_t *pids;
	if (_main_fork(&pids)) {
		DEBUG("Children forked");
	} else {
		ERROR("Could not fork children.");
		return 1;
	}
	
	if (apps_postfork()) {
		DEBUG("Postfork apps done");
	} else {
		ERROR("Error with apps postfork");
		_main_cull_children(pids);
		return 1;
	}
	
	printf("READY\n");
	fflush(stdout);
	
	// The main thread just sits here, waiting
	// The children processes will never get here
	int count = option_processes();
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
