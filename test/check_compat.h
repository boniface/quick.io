/**
 * Older versions of check don't support the new macros; this file just
 * monkeys them onto it.
 * @file
 */

#pragma once

#if CHECK_MAJOR_VERSION == 0 && CHECK_MINOR_VERSION == 9 && CHECK_MICRO_VERSION < 10

#include <check.h>

#undef _ck_assert_int
#undef ck_assert_int_eq
#undef ck_assert_int_ne
#undef ck_assert_int_lt
#undef ck_assert_int_le
#undef ck_assert_int_gt
#undef ck_assert_int_ge

#undef _ck_assert_uint
#undef ck_assert_uint_eq
#undef ck_assert_uint_ne
#undef ck_assert_uint_lt
#undef ck_assert_uint_le
#undef ck_assert_uint_gt
#undef ck_assert_uint_ge

#undef _ck_assert_str
#undef ck_assert_str_eq
#undef ck_assert_str_ne
#undef ck_assert_str_lt
#undef ck_assert_str_le
#undef ck_assert_str_gt
#undef ck_assert_str_ge

#undef _ck_assert_ptr
#undef ck_assert_ptr_eq
#undef ck_assert_ptr_ne

#define _ck_assert_int(X, OP, Y) do { \
  intmax_t _ck_x = (X); \
  intmax_t _ck_y = (Y); \
  ck_assert_msg(_ck_x OP _ck_y, "Assertion '"#X#OP#Y"' failed: "#X"==%jd, "#Y"==%jd", _ck_x, _ck_y); \
} while (0)
#define ck_assert_int_eq(X, Y) _ck_assert_int(X, ==, Y)
#define ck_assert_int_ne(X, Y) _ck_assert_int(X, !=, Y)
#define ck_assert_int_lt(X, Y) _ck_assert_int(X, <, Y)
#define ck_assert_int_le(X, Y) _ck_assert_int(X, <=, Y)
#define ck_assert_int_gt(X, Y) _ck_assert_int(X, >, Y)
#define ck_assert_int_ge(X, Y) _ck_assert_int(X, >=, Y)

#define _ck_assert_uint(X, OP, Y) do { \
  uintmax_t _ck_x = (X); \
  uintmax_t _ck_y = (Y); \
  ck_assert_msg(_ck_x OP _ck_y, "Assertion '"#X#OP#Y"' failed: "#X"==%ju, "#Y"==%ju", _ck_x, _ck_y); \
} while (0)
#define ck_assert_uint_eq(X, Y) _ck_assert_uint(X, ==, Y)
#define ck_assert_uint_ne(X, Y) _ck_assert_uint(X, !=, Y)
#define ck_assert_uint_lt(X, Y) _ck_assert_uint(X, <, Y)
#define ck_assert_uint_le(X, Y) _ck_assert_uint(X, <=, Y)
#define ck_assert_uint_gt(X, Y) _ck_assert_uint(X, >, Y)
#define ck_assert_uint_ge(X, Y) _ck_assert_uint(X, >=, Y)

#define _ck_assert_str(X, OP, Y) do { \
  const char* _ck_x = (X); \
  const char* _ck_y = (Y); \
  ck_assert_msg(0 OP strcmp(_ck_y, _ck_x), \
    "Assertion '"#X#OP#Y"' failed: "#X"==\"%s\", "#Y"==\"%s\"", _ck_x, _ck_y); \
} while (0)
#define ck_assert_str_eq(X, Y) _ck_assert_str(X, ==, Y)
#define ck_assert_str_ne(X, Y) _ck_assert_str(X, !=, Y)
#define ck_assert_str_lt(X, Y) _ck_assert_str(X, <, Y)
#define ck_assert_str_le(X, Y) _ck_assert_str(X, <=, Y)
#define ck_assert_str_gt(X, Y) _ck_assert_str(X, >, Y)
#define ck_assert_str_ge(X, Y) _ck_assert_str(X, >=, Y)

#define _ck_assert_ptr(X, OP, Y) do { \
  void* _ck_x = (X); \
  void* _ck_y = (Y); \
  ck_assert_msg(_ck_x OP _ck_y, "Assertion '"#X#OP#Y"' failed: "#X"==%p, "#Y"==%p", _ck_x, _ck_y); \
} while (0)
#define ck_assert_ptr_eq(X, Y) _ck_assert_ptr(X, ==, Y)
#define ck_assert_ptr_ne(X, Y) _ck_assert_ptr(X, !=, Y)

#endif
