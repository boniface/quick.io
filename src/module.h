/**
 * @file module.h
 *
 * For making functions visible to modules.
 * The single macro for making functions visible to loaded modules (aka. apps).
 */
#pragma once

/**
 * Make a function visible in a module.
 * By decorating a function with this, it will be visible in any loaded application.
 *
 * @see http://www.gnu.org/software/gnulib/manual/html_node/Exported-Symbols-of-Shared-Libraries.html
 */
#define APP_EXPORT __attribute__((__visibility__("default")))

/**
 * The functions available to modules (aka apps).
 *
 * @defgroup AppFunctions App-visible functions
 *
 * Any function in this group can be called by any app. Be sure to pay attention
 * to any warnings about thread safety.
 */
