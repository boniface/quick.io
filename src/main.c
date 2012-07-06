#include <glib.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "apps.h"
#include "commands.h"
#include "debug.h"
#include "gossip.h"
#include "option.h"
#include "pubsub.h"
#include "socket.h"

/**
 * Fork all of the children processes.
 *
 * *pipes will be set to the write pipes into the children.
 */
static gboolean _main_fork(int **pipes, pid_t **pids) {
	gint processes = option_processes();
	
	*pipes = malloc(processes * sizeof(**pipes));
	*pids = malloc(processes * sizeof(**pids));
	
	// If this malloc fails, shit is fucked
	if (pipes == NULL) {
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
			// We can't write back to the parent, so close that pipe
			close(pipefd[1]);
			
			// Get epoll going.  If this fails...shit.
			if (!socket_init_process()) {
				ERRORF("Epoll init failed in #%d", processes);
				exit(1);
			}
			
			if (!apps_run()) {
				ERRORF("Could not init apps in #%d.", processes);
				exit(1);
			}
			
			// Setup our gossip stuff
			gossip_client(pipefd[0]);
			
			// Run the socket loop
			socket_loop();
		} else {
			// We're the parent, so close the reading pipe, we only write to the child
			close(pipefd[0]);
			
			// Save the writing pipe for later gossip communication
			*(*pipes - processes - 1) = pipefd[1];
			
			// Save the process id for later culling
			*(*pids - processes - 1) = pid;
		}
	}
	
	return TRUE;
}

static void _main_cull_children(int *pids) {
	DEBUG("All children are being culled...");
	
	int count = option_processes();
	while (count-- > 0) {
		kill(*(pids + count - 1), SIGINT);
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
	
	int *pipes;
	pid_t *pids;
	if (_main_fork(&pipes, &pids)) {
		DEBUG("Children forked.");
	} else {
		ERROR("Could not fork children.");
		return 1;
	}
	
	printf("READY\n");
	fflush(stdout);
	
	// If the gossip listener fails to start, kill the children and leave
	if (!gossip_server(pipes)) {
		_main_cull_children(pids);
		ERROR("Gossip server failed to listen.");
		return 1;
	}
	
	// If we ever get here...we're not a forking server
	// Make sure all the children are dead
	_main_cull_children(pids);
	
	return 0;
}
