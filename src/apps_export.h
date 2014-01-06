/**
 * There are only wrapper funtions here. All functions are defined elsewhere and
 * found in struct qio_export to be exported.
 * @file apps_export.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#pragma once

/**
 * Gets all of the functions exported from QIO for apps
 */
struct qio_exports apps_export_get_fns();
