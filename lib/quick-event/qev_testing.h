#undef QEV_TEST_LOCK
#define QEV_TEST_LOCK(name) \
	name ## _waiting++; \
	g_mutex_lock(&name); \
	name ## _waiting--; \
	g_mutex_unlock(&name);

#define QEV_TEST_MUTEX(name) \
	static guint name ## _waiting = 0; \
	static GMutex name;

#define QEV_TEST_WAIT(name, until) \
	while (name ## _waiting < until)

QEV_TEST_MUTEX(qev_client_read_in)
QEV_TEST_MUTEX(qev_close_before)