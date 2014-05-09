quickio-testapps
================

.. program:: quickio-testapps

Synopsis
--------

quickio-testapps [-p /usr/bin/quickio] [-a test_app.so] [-a /usr/lib/quickio/test_app.so]

Description
-----------

Runs the application test suite against all QuickIO apps found. By default, searches in the current directory for anything with the pattern test_*.so.

Options
-------

.. option:: -a <application>, --app=<application>

	Which application to run the test suite on. You can specify numerous applications to run them all in one go.

.. option:: -p <quickio path>, --quickio-path=<quickio path>

	Absolute path to the quickio binary, only needed if it's not in your PATH.

.. include:: bugs.rst
