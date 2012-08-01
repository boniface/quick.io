#pragma once

// For allowing a function to be visible inside a module, by default, everything is hidden
// See method 3 at: http://www.gnu.org/software/gnulib/manual/html_node/Exported-Symbols-of-Shared-Libraries.html
#define MODULE_EXPORT __attribute__((__visibility__("default")))