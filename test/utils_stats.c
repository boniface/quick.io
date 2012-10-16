/**
 * The shared memory for the running process in the test.
 */

#include "utils_stats.h"

static int _statsfd;
static test_stats_t *_stats = NULL;

void utils_stats_setup() {
	_statsfd = shm_open(TEST_SHM_NAME, SHM_FLAGS, SHM_MASK);
	test(_statsfd, "SHM Open");
	test(ftruncate(_statsfd, sizeof(*_stats)) == 0, "Stats sized");
	_stats = mmap(NULL, sizeof(*_stats), PROT_READ | PROT_WRITE, MAP_SHARED, _statsfd, 0);
	memset(_stats, 0, sizeof(*_stats));
}

void utils_stats_teardown() {
	shm_unlink(TEST_SHM_NAME);
}

test_stats_t* utils_stats() {
	return _stats;
}