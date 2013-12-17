/**
 * There are no functions here. All functions are defined in quickio_app.h
 * since they need to be visible to the app.
 * @file apps_export.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#pragma once

#undef QIO_EXPORT

/**
 * Makes functions visible to apps
 */
#define QIO_EXPORT __attribute__((__visibility__("default")))
