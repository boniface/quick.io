/**
 * The shared memory for the running process in the test.
 */

#include "utils_stats.h"

static test_stats_t *_stats = NULL;

void utils_stats_setup()
{
	int statsfd = shm_open(TEST_SHM_NAME, SHM_FLAGS, SHM_MASK);
	check(statsfd, "SHM Open");
	check(ftruncate(statsfd, sizeof(*_stats)) == 0, "Stats sized");
	_stats = mmap(NULL, sizeof(*_stats), PROT_READ | PROT_WRITE,
				MAP_SHARED, statsfd, 0);
	memset(_stats, 0, sizeof(*_stats));

	statsfd = shm_open(SHM_NAME, SHM_FLAGS, SHM_MASK);
	check(statsfd, "SHM Open");
	check(ftruncate(statsfd, sizeof(*stats)) == 0, "Stats sized");
	stats = mmap(NULL, sizeof(*stats), PROT_READ | PROT_WRITE,
				MAP_SHARED, statsfd, 0);
	memset(stats, 0, sizeof(*stats));
}

void utils_stats_teardown()
{
	shm_unlink(TEST_SHM_NAME);
	shm_unlink(SHM_NAME);
}

test_stats_t* utils_stats()
{
	return _stats;
}
