/**
 * Various macro definitions to make testing simpler, with nicer messages.
 * Includes all the necessary includes to write test files.
 * @file test.h
 */
#pragma once
#include <check.h>
#include <fcntl.h>
#include <libgen.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "qio.h"
#include "utils.h"

// Shortcuts live here for app testing, include so we don't duplicate code
#include "../app/quickio_app_test.h"
