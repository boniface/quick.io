#include <assert.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glob.h>
#include <gmodule.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define INI_FILE "quickio_testapps.ini"
#define INI \
	"[quick-event]\n" \
	"threads = 2\n" \
	"[quick.io]\n" \
	"bind-address = 0.0.0.0\n" \
	"bind-port = 55439\n" \
	"run-app-tests = true\n" \
	"[quick.io-apps]\n" \
	"app = %s/%s\n"

#define GLOB_PATTERN "test_*.so"

static gchar **_apps = NULL;
static gchar *_quickio_path = NULL;

static GOptionEntry command_options[] = {
	{"app", 'a', 0, G_OPTION_ARG_FILENAME_ARRAY, &_apps,
		"Which app to run the tests on. May be given more than once.",
		"./test_app.so"},
	{"quickio-path", 'p', 0, G_OPTION_ARG_FILENAME, &_quickio_path,
		"Absolute path to the quickio binary; searches the path if not given.",
		"/usr/bin/quickio"},
	{NULL, 0, 0, 0, NULL, NULL, NULL},
};

static gboolean _parse_args(int argc, gchar **argv)
{
	GError *error = NULL;
	GOptionContext *context = g_option_context_new("");
	g_option_context_add_main_entries(context, command_options, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		goto error;
	}

	if (_quickio_path == NULL) {
		_quickio_path = "quickio";
	}

	// If no apps given, we default to all in the current directory
	if (_apps == NULL) {
		glob_t gb;
		if (glob(GLOB_PATTERN, 0, NULL, &gb) == 1) {
			perror("Error globbing for " GLOB_PATTERN);
			goto error;
		}

		_apps = g_strdupv(gb.gl_pathv);
		globfree(&gb);
	}

	return TRUE;

error:
	if (error != NULL) {
		fprintf(stderr, "Error: %s\n", error->message);
		g_error_free(error);
	}

	return FALSE;
}

int main(int argc, char **argv)
{
	gchar *cwd = g_get_current_dir();
	GString *buff = g_string_sized_new(1024);

	if (!_parse_args(argc, argv)) {
		return 1;
	}

	if (_apps == NULL || g_strv_length(_apps) == 0) {
		fprintf(stderr, "No applications found to test "
						"(looking for: " GLOB_PATTERN ").\n");
		return 2;
	}

	for (guint i = 0; i < g_strv_length(_apps); i++) {
		gboolean ok;
		gint exit_status;
		GError *error = NULL;
		gchar *app = _apps[i];

		g_string_printf(buff, INI, cwd, app);
		g_file_set_contents(INI_FILE, buff->str, buff->len, NULL);

		g_string_printf(buff, "Running app tests on: %s", app);
		gchar *fill = g_strnfill(buff->len + 4, '*');

		printf("\n\n%s\n", fill);
		printf("  %s\n", buff->str);
		printf("%s\n\n", fill);

		g_free(fill);

		gchar *argv[] = {
			_quickio_path,
			"--config-file=" INI_FILE,
			NULL,
		};

		ok = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
								NULL, NULL, &exit_status, &error);
		g_unlink(INI_FILE);

		if (!ok) {
			fprintf(stderr, "Error: %s\n", error->message);
			return 3;
		}

		if (exit_status != 0) {
			fprintf(stderr, "\nError: tests failed on \"%s\" "
						"(exit status %d). Exiting.\n",
						app, exit_status);
			return 3;
		}
	}

	g_string_free(buff, TRUE);
	g_free(cwd);

	return 0;
}