/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

/**
 * Check doesn't make its own copy of this guy...
 */
static gchar *_xml_file = NULL;

void test_new(
	const gchar *suite_name,
	SRunner **sr,
	Suite **s)
{
	_xml_file = g_strdup_printf("test_%s.xml", suite_name);

	*s = suite_create(suite_name);
	*sr = srunner_create(*s);
	srunner_set_xml(*sr, _xml_file);
}

gboolean test_do(SRunner *sr)
{
	gint failures;

	srunner_run_all(sr, CK_NORMAL);
	failures = srunner_ntests_failed(sr);
	srunner_free(sr);

	g_free(_xml_file);

	return failures > 0;
}

void test_setup()
{
	qio_main(0, NULL);
}

void test_teardown()
{
	qev_exit();
}

TCase* test_add(Suite *s, const gchar *name, ...)
{
	va_list args;
	TFun tfun;
	TCase *tc = tcase_create(name);

	tcase_add_checked_fixture(tc, test_setup, test_teardown);
	suite_add_tcase(s, tc);

	va_start(args, name);

	while (TRUE) {
		tfun = va_arg(args, TFun);

		if (tfun == NULL) {
			break;
		}

		tcase_add_test(tc, tfun);
	}

	va_end(args);

	return tc;
}
