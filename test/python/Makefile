.PHONY: clean test

all: test

test:
	ve/bin/nosetests --processes=8 --process-restartworker

test-xunit:
	ve/bin/nosetests --with-xunit --xunit-file=../../unit_py.xml

clean:
	rm ../../unit_py.xml
	rm *.pyc