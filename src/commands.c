#include <string.h>

#include "client.h"
#include "commands.h"

/**
 * The command function type.
 */
typedef short(*commandfn_t)(command_t*);

/**
 * A table of all of the commands that a client can send.
 */
static GHashTable* commands;

/**
 * The "ping:" command
 */
static short command_ping(command_t *command) {
	//This command just needs to return whatever text we were given
	//-1 -> get rid of the null pointer from sizeof()
	g_string_erase(command->buffer, 0, sizeof(COMMAND_PING)-1);
	return CLIENT_GOOD;
}

short command_handle(command_t *command) {
	GString *buff = command->buffer;
	int delim = strcspn(buff->str, COMMAND_DELIMITER);
	
	// If no command was found (ie. no delimiter, nothing before the delimeter)
	if (delim == buff->len) {
		return CLIENT_UNKNOWN_COMMAND;
	}
	
	char cmd_s[delim+1];
	cmd_s[delim] = 0; // Null-terminate it
	strncpy(cmd_s, buff->str, delim);
	
	// Get the command from the hash table of commands
	commandfn_t fn = g_hash_table_lookup(commands, cmd_s);
	if (fn == NULL) {
		return CLIENT_UNKNOWN_COMMAND;
	}
	
	return (*fn)(command);
}

gboolean commands_init() {
	commands = g_hash_table_new(g_str_hash, g_str_equal);
	
	g_hash_table_insert(commands, "ping", command_ping);
	
	return TRUE;
}