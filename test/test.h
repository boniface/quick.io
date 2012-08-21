/**
 * Various macro definitions to make testing simpler, with nicer messages.
 * Includes all the necessary includes to write test files.
 * @file test.h
 */
#pragma once
#include <check.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "qio.h"
#include "utils.h"

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
	size_t desc_len = 0, sep_len = 0; \
	if (*description != '\0') { \
		desc_len = strlen(description); \
		strncpy(msg, description, sizeof(msg)); \
		sep_len = sizeof(SEPARATOR)-1; \
		strncat(msg + desc_len, SEPARATOR, sizeof(msg) - desc_len); \
	} \
	snprintf(msg + desc_len + sep_len, sizeof(msg) - desc_len - sep_len, format, actual, expect);

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
	test_str_eq(test, should_be, "")

/**
 * Tests if the binary blocks are equal, with a message if something goes wrong.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param length The length of the binary blob.
 * @param description The description of the test, to be printed with failure information
 */
#define test_bin_eq(test, should_be, length, description) { \
	_test_setup(char*, test, should_be); \
	_msg("The binary blobs do not match: '%X'!='%X'", *actual, *expect, description); \
	_fail_unless(memcmp(actual, expect, length) == 0, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two strings are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param length The length of the binary blob.
 */
#define test_bin_eq_(test, should_be, length) \
	test_bin_eq(test, should_be, length, "")

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
	_fail_unless(actual == expect, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two chars are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_char_eq_(test, should_be) \
	test_char_eq(test, should_be, "")

/**
 * Tests if the two int16s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_int16_eq(test, should_be, description) { \
	_test_setup(gint16, test, should_be); \
	_msg("Assertion %"G_GINT16_FORMAT"==%"G_GINT16_FORMAT" failed", actual, expect, description); \
	_fail_unless(actual == expect, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two int16s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_int16_eq_(test, should_be) \
	test_int16_eq(test, should_be, "")

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
	_fail_unless(actual == expect, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two uint16s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_uint16_eq_(test, should_be) \
	test_uint16_eq(test, should_be, "")

/**
 * Tests if the two int32s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_int32_eq(test, should_be, description) { \
	_test_setup(gint32, test, should_be); \
	_msg("Assertion %"G_GINT32_FORMAT"==%"G_GINT32_FORMAT" failed", actual, expect, description); \
	_fail_unless(actual == expect, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two int32s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_int32_eq_(test, should_be) \
	test_int32_eq(test, should_be, "")

/**
 * Tests if the two uint32s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_uint32_eq(test, should_be, description) { \
	_test_setup(guint32, test, should_be); \
	_msg("Assertion %"G_GUINT32_FORMAT"==%"G_GUINT32_FORMAT" failed", actual, expect, description); \
	_fail_unless(actual == expect, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two uint32s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_uint32_eq_(test, should_be) \
	test_uint32_eq(test, should_be, "")

/**
 * Tests if the two int64s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_int64_eq(test, should_be, description) { \
	_test_setup(gint64, test, should_be); \
	_msg("Assertion %"G_GINT64_FORMAT"==%"G_GINT64_FORMAT" failed", actual, expect, description); \
	_fail_unless(actual == expect, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two int64s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_int64_eq_(test, should_be) \
	test_int64_eq(test, should_be, "")

/**
 * Tests if the two uint64s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_uint64_eq(test, should_be, description) { \
	_test_setup(guint64, test, should_be); \
	_msg("Assertion %"G_GUINT64_FORMAT"==%"G_GUINT64_FORMAT" failed", actual, expect, description); \
	_fail_unless(actual == expect, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two uint64s are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_uint64_eq_(test, should_be) \
	test_uint64_eq(test, should_be, "")

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
	_fail_unless(actual == expect, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two size_t's are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_size_eq_(test, should_be) \
	test_size_eq(test, should_be, "")

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
	test_int32_eq(test, should_be, "")

/**
 * Tests if the two pointers are equal.
 * 
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 * @param description The description of the test, to be printed with failure information
 */
#define test_ptr_eq(test, should_be, description) { \
	_test_setup(void*, test, should_be); \
	_msg("Assertion %p==%p failed", actual, expect, description); \
	_fail_unless(actual == expect, __FILE__, __LINE__, msg, NULL);}

/**
 * Tests if the two pointers are equal.
 *
 * @param test The thing to test.
 * @param should_be What the thing is expected to be.
 */
#define test_ptr_eq_(test, should_be) \
	test_ptr_eq(test, should_be, "")
