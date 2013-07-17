/**
 * Includes a bunch of shortcuts for test cases.
 * @file qio_app_test.h
 */

#ifndef QIO_APP_TEST_H
#define QIO_APP_TEST_H

#define APP_TESTING

#include <stdio.h>

/**
 * Milliseconds to microseconds
 */
#define MS_TO_USEC(ms) (ms * 1000)

/**
 * Milliseconds to nanoseconds
 */
#define MS_TO_NSEC(ms) (MS_TO_USEC(ms) * 1000)

/**
 * Seconds to microseconds
 */
#define SEC_TO_USEC(secs) MS_TO_USEC(secs * 1000)

/**
 * Test that the assertion is true.
 */
#define check fail_unless

/**
 * Test that an assertion is false.
 * This is a basic wrapper around fail_if().
 */
#define check_not fail_if

/**
 * The separator for the system error message and the user error message
 */
#define SEPARATOR " -- "

/**
 * Setup the msg format and protect the test and should_be parameters from
 * being called multiple times if they're functions.
 *
 * @param type The type of things being compared.
 * @param test The test argument from the macro.
 * @param should_be The should_be argument from the macro.
 */
#define _qio_check_setup(type, test, should_be) \
	char _qio_msg[BUFSIZ]; \
	type actual = test; \
	type expect = should_be;

/**
 * Formats the error message that goes along with the text.
 *
 * @param format The message format
 * @param actual What the result is
 * @param expect What was expected
 * @param description The description of the test, to be printed with failure information
 */
#define _qio_msg(format, actual, expect, description) \
	size_t desc_len = 0, sep_len = 0; \
	if (*description != '\0') { \
		desc_len = strlen(description); \
		strncpy(_qio_msg, description, sizeof(_qio_msg)); \
		sep_len = sizeof(SEPARATOR)-1; \
		strncat(_qio_msg + desc_len, SEPARATOR, sizeof(_qio_msg) - desc_len); \
	} \
	snprintf(_qio_msg + desc_len + sep_len, sizeof(_qio_msg) - desc_len - sep_len, format, actual, expect);

/**
 * Given the asynchronous nature of QIO, it is often necessary to wait for a specific
 * condition to be satisfied before continuing. This macro will wait for ~1 second for the
 * condition to be satisfied before failing the test. As a byproduct, this macro will also
 * ensure that `cond` is met before continuing the test, so there is no need to use any other
 * check_* macro to verify the condition after this macro is used.
 */
#define WAIT_FOR_TIME(cond, secs) \
	{ \
		for (guint i = 0; i < (guint)((secs) * 1000); i++) { \
			if ((cond)) { \
				break; \
			} \
			g_usleep(1000); \
		} \
		check(cond); \
	}

/**
 * Like WAIT_FOR_TIME, except you can specify the number of seconds to wait.
 *
 * @see WAIT_FOR_TIME
 */
#define WAIT_FOR(cond) \
	WAIT_FOR_TIME(cond, 1)

/**
 * Tests if the two strings are equal, with a message if something goes wrong.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_str_eq(test, should_be, description) { \
	_qio_check_setup(const char*, test, should_be); \
	_qio_msg("Assertion '%s'=='%s' failed", actual, expect, description); \
	if (actual == NULL || expect == NULL) { \
		fail_unless(actual == expect, _qio_msg, NULL); \
	} else { \
		fail_unless(strcmp(actual, expect) == 0, _qio_msg, NULL); \
	} }

/**
 * Tests if the two strings are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_str_eq_(test, should_be) \
	check_str_eq(test, should_be, "")

/**
 * Tests if the binary blocks are equal, with a message if something goes wrong.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param length The length of the binary blob.
 * @param description The description of the test, to be printed with failure information
 */
#define check_bin_eq(test, should_be, length, description) { \
	_qio_check_setup(char*, test, should_be); \
	_qio_msg("The binary blobs do not match: '%X'!='%X'", (*actual) & 0xFF, (*expect) & 0xFF, description); \
	fail_unless(memcmp(actual, expect, length) == 0, _qio_msg, NULL);}

/**
 * Tests if the two strings are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param length The length of the binary blob.
 */
#define check_bin_eq_(test, should_be, length) \
	check_bin_eq(test, should_be, "")

/**
 * Tests if the two chars are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_char_eq(test, should_be, description) { \
	_qio_check_setup(gchar, test, should_be); \
	_qio_msg("Assertion (character) %d==%d failed", actual, expect, description); \
	fail_unless(actual == expect, _qio_msg, NULL);}

/**
 * Tests if the two chars are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_char_eq_(test, should_be) \
	check_char_eq(test, should_be, "")


/**
 * Tests if the two int16s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_int16_eq(test, should_be, description) { \
	_qio_check_setup(gint16, test, should_be); \
	_qio_msg("Assertion %"G_GINT16_FORMAT"==%"G_GINT16_FORMAT" failed", actual, expect, description); \
	fail_unless(actual == expect, _qio_msg, NULL);}

/**
 * Tests if the two int16s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_int16_eq_(test, should_be) \
	check_int16_eq(test, should_be, "")

/**
 * Tests if the two uint16s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_uint16_eq(test, should_be, description) { \
	_qio_check_setup(guint16, test, should_be); \
	_qio_msg("Assertion %"G_GUINT16_FORMAT"==%"G_GUINT16_FORMAT" failed", actual, expect, description); \
	fail_unless(actual == expect, _qio_msg, NULL);}

/**
 * Tests if the two uint16s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_uint16_eq_(test, should_be) \
	check_uint16_eq(test, should_be, "")

/**
 * Tests if the two int32s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_int32_eq(test, should_be, description) { \
	_qio_check_setup(gint32, test, should_be); \
	_qio_msg("Assertion %"G_GINT32_FORMAT"==%"G_GINT32_FORMAT" failed", actual, expect, description); \
	fail_unless(actual == expect, _qio_msg, NULL);}

/**
 * Tests if the two int32s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_int32_eq_(test, should_be) \
	check_int32_eq(test, should_be, "")

/**
 * Tests if the two uint32s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_uint32_eq(test, should_be, description) { \
	_qio_check_setup(guint32, test, should_be); \
	_qio_msg("Assertion %"G_GUINT32_FORMAT"==%"G_GUINT32_FORMAT" failed", actual, expect, description); \
	fail_unless(actual == expect, _qio_msg, NULL);}

/**
 * Tests if the two uint32s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_uint32_eq_(test, should_be) \
	check_uint32_eq(test, should_be, "")

/**
 * Tests if the two int64s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_int64_eq(test, should_be, description) { \
	_qio_check_setup(gint64, test, should_be); \
	_qio_msg("Assertion %"G_GINT64_FORMAT"==%"G_GINT64_FORMAT" failed", actual, expect, description); \
	fail_unless(actual == expect, _qio_msg, NULL);}

/**
 * Tests if the two int64s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_int64_eq_(test, should_be) \
	check_int64_eq(test, should_be, "")

/**
 * Tests if the two uint64s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_uint64_eq(test, should_be, description) { \
	_qio_check_setup(guint64, test, should_be); \
	_qio_msg("Assertion %"G_GUINT64_FORMAT"==%"G_GUINT64_FORMAT" failed", actual, expect, description); \
	fail_unless(actual == expect, _qio_msg, NULL);}

/**
 * Tests if the two uint64s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_uint64_eq_(test, should_be) \
	check_uint64_eq(test, should_be, "")

/**
 * Tests if the two size_t's are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_size_eq(test, should_be, description) { \
	_qio_check_setup(size_t, test, should_be); \
	_qio_msg("Assertion %zd==%zd failed", actual, expect, description); \
	fail_unless(actual == expect, _qio_msg, NULL);}

/**
 * Tests if the two size_t's are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_size_eq_(test, should_be) \
	check_size_eq(test, should_be, "")

/**
 * Tests if the two enum status's match.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_status_eq(test, should_be, description) { \
	_qio_check_setup(enum status, test, should_be); \
	_qio_msg("Assertion %s==%s failed", test_status_to_str(actual), test_status_to_str(expect), description); \
	fail_unless(actual == expect, _qio_msg, NULL);}

/**
 * Tests if the two enum status's match.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_status_eq_(test, should_be) \
	check_int32_eq(test, should_be, "")

/**
 * Tests if the two pointers are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define check_ptr_eq(test, should_be, description) { \
	_qio_check_setup(const void*, test, should_be); \
	_qio_msg("Assertion %p==%p failed", actual, expect, description); \
	fail_unless(actual == expect, _qio_msg, NULL);}

/**
 * Tests if the two pointers are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define check_ptr_eq_(test, should_be) \
	check_ptr_eq(test, should_be, "")

#endif
