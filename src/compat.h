/**
 * For older versions of libraries, adds helpers for things that don't exist
 * @file compat.h
 */

#pragma once
#include "quickio.h"

#ifndef G_DEFINE_QUARK
	#define G_DEFINE_QUARK(QN, q_n)                                         \
	GQuark                                                                  \
	q_n##_quark (void)                                                      \
	{                                                                       \
	  static GQuark q;                                                      \
	  if G_UNLIKELY (q == 0)                                                \
	    q = g_quark_from_static_string (#QN);                               \
	  return q;                                                             \
	}
#endif
