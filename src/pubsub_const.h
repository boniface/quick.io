#include <sys/epoll.h>

// The number of threads to keep in the pool
#define LISTEN_THREADS 2

// Events epoll should wait for
#define EPOLL_READ_EVENTS EPOLLIN | EPOLLRDHUP | EPOLLET
#define EPOLL_MAX_EVENTS 100

// The hint to GPtrArray for initial size
#define INITIAL_ROOM_SIZE 1000

// Commands that can be passed down to the clients
#define QUERY_READY "ready"