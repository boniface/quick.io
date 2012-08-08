/**
 * Various macro definitions to make testing simpler, with nicer messages.
 * Includes all the necessary includes to write test files.
 * @file test.h
 */
#pragma once
#include <check.h>
#include "qio.h"
#include "test_utils.h"

/**
 * Convert a status_t to a string, for printing.
 *
 * @param status The status to look for.
 *
 * @return A printable string that names the status, or NULL if not found / multiple statuses.
 */
char* test_status_to_str(status_t status);

/**
 * Test that the assertion is true.
 * This is a basic wrapper around fail_unless().
 */
#define test fail_unless

/**
 * Test that an assertion is false.
 * This is a basic wrapper around fail_if().
 */
#define test_not fail_if

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
#define _test_setup(type, test, should_be) \
	char msg[BUFSIZ]; \
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
#define _msg(format, actual, expect, description) \
	size_t desclen = 0, seplen = 0; \
	if (description != NULL) { \
		desclen = strlen(description); \
		strncpy(msg, description, sizeof(msg)); \
		seplen = sizeof(SEPARATOR)-1; \
		strncat(msg + desclen, SEPARATOR, sizeof(msg) - desclen); \
	} \
	snprintf(msg + desclen + seplen, sizeof(msg) - desclen - seplen, format, actual, expect);

/**
 * Tests if the two strings are equal, with a message if something goes wrong.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_str_eq(test, should_be, description) { \
	_test_setup(char*, test, should_be); \
	_msg("Assertion '%s'=='%s' failed", actual, expect, description); \
	_fail_unless(strcmp(actual, expect) == 0, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two strings are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_str_eq_(test, should_be) \
	test_str_eq(test, should_be, NULL)

/**
 * Tests if the two chars are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_char_eq(test, should_be, description) { \
	_test_setup(gchar, test, should_be); \
	_msg("Assertion (character) %d==%d failed", actual, expect, description); \
	_fail_unless(test == should_be, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two chars are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_char_eq_(test, should_be) \
	test_char_eq(test, should_be, NULL)

/**
 * Tests if the two ints are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_int32_eq(test, should_be, description) { \
	_test_setup(gint32, test, should_be); \
	_msg("Assertion %d==%d failed", actual, expect, description); \
	_fail_unless(test == should_be, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two ints are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_int32_eq_(test, should_be) \
	test_int32_eq(test, should_be, NULL)

/**
 * Tests if the two size_t's are equal.
 * 
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_size_eq(test, should_be, description) { \
	_test_setup(size_t, test, should_be); \
	_msg("Assertion %zd==%zd failed", actual, expect, description); \
	_fail_unless(test == should_be, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two size_t's are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_size_eq_(test, should_be) \
	test_size_eq(test, should_be, NULL)

/**
 * Tests if the two uint16s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_uint16_eq(test, should_be, description) { \
	_test_setup(guint16, test, should_be); \
	_msg("Assertion %"G_GUINT16_FORMAT"==%"G_GUINT16_FORMAT" failed", actual, expect, description); \
	_fail_unless(test == should_be, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two uint16s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_uint16_eq_(test, should_be) \
	test_uint16_eq(test, should_be, NULL)

/**
 * Tests if the two status_t's match.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_status_eq(test, should_be, description) { \
	_test_setup(status_t, test, should_be); \
	_msg("Assertion %s==%s failed", test_status_to_str(actual), test_status_to_str(expect), description); \
	_fail_unless(actual == expect, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two status_t's match.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_status_eq_(test, should_be) \
	test_int32_eq(test, should_be, NULL)
