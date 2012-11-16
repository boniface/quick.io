pound: pound.c
	gcc -std=gnu99 -Wall -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include $< -lglib-2.0 -levent -o $@

pound_reconnect: pound_reconnect.c
	gcc -std=gnu99 -Wall -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include $< -lglib-2.0 -levent -o $@

benchmark_epoll: benchmark_epoll.c
	gcc -std=gnu99 -Wall -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include $< -lglib-2.0 -o $@

benchmark_libevent: benchmark_libevent.c
	gcc -std=gnu99 -Wall $< -levent -o $@

clean:
	rm -f pound
	rm -f benchmark_epoll
	rm -f benchmark_libevent