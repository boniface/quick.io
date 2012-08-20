#include "test.h"

/**
 * The file that we puts locks on.
 */
static sem_t *_sem;

gboolean test_lock_init() {
	test_lock_close();
	_sem = sem_open(SEM_NAME, O_CREAT, S_IRUSR|S_IWUSR, 1);
	
	if (_sem == SEM_FAILED) {
		ERRORF("Error initing test_lock: %s", strerror(errno));
		return FALSE;
	}
	
	return TRUE;
}

void test_lock_close() {
	sem_unlink(SEM_NAME);
}

void test_lock_acquire() {
	sem_wait(_sem);
}

void test_lock_release() {
	sem_post(_sem);
}