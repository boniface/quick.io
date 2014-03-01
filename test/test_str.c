/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

const gchar *_good[] = {
	"test",
	"\u00c4\u00df\u01b6\u2260\u00f6", // Äßƶ≠ö
	"\u00ef\u01b6",
	"\u00ff\u01b6",
};

const gchar *_bad[] = {
	"\xf3\x3d\u01b6",
};

START_TEST(test_str)
{
	guint i;
	for (i = 0; i < G_N_ELEMENTS(_good); i++) {
		const gchar *str = _good[i];
		ck_assert_msg(qio_str_is_utf8((guchar*)str, strlen(str)),
						"Didn't like (%d): %s", i, str);
	}

	for (i = 0; i < G_N_ELEMENTS(_bad); i++) {
		const gchar *str = _bad[i];
		ck_assert_msg(!qio_str_is_utf8((guchar*)str, strlen(str)),
						"Liked when it shouldn't (%d): %s", i, str);
	}
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("str", &sr, &s);

	tcase = tcase_create("Sane");
	suite_add_tcase(s, tcase);
	tcase_add_test(tcase, test_str);

	return test_do(sr);
}
