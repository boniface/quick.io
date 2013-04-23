run: ve
	ve/bin/python server/ dev.ini

test: ve
	ve/bin/nosetests server/

ve: requirements.txt
	test -d ve || virtualenv ve
	ve/bin/pip install -r requirements.txt
