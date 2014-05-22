/**
 * Periodically runs tasks on every client.
 * @file
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#pragma once
#include "quickio.h"

/**
 * How often periodic jobs run
 */
#define PERIODIC_INTERVAL 10

/**
 * The periodic interval in usec
 */
#define PERIODIC_INTERVAL_USEC QEV_SEC_TO_USEC(PERIODIC_INTERVAL)

/**
 * Run periodic tasks immediately.
 */
void periodic_run();

/**
 * Initialize the periodic task runner.
 */
void periodic_init();
