.PHONY: http-parser

pound: pound.c
	gcc -std=gnu99 -Wall -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include $< -lglib-2.0 -levent -o $@

http-parser:
	$(MAKE) -C http-parser

clean:
	$(MAKE) -C http-parser clean
	rm -f pound