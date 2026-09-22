
#include "func.h"
#include <sched.h>

size_t	get_time(void)
{
	struct timeval	time_stuct;

	if (gettimeofday(&time_stuct, NULL) == -1)
		return (ende("gettimeofday() not working, tf u want"), 0);
	return (time_stuct.tv_sec * 1000 + time_stuct.tv_usec / 1000);
}

void	eepy(size_t ms)
{
	size_t	bed_time;

	bed_time = get_time();
	while (ms > (get_time() - bed_time))
		usleep(100);
}
