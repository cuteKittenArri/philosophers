#include "func.h"
#include "philomilo.h"
#include <linux/limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <time.h>

static bool	u_good(t_philo *philo)
{
	size_t	now;

	now = get_time();
	if ((now - philo->last_nom) > philo->times.die)
		return (false);
	else
		return (true);
}

static void	death_checker(t_env *env)
{
	int		i;
	bool	alive;
	int		full;

	full = 0;
	i = 0;
	while (full < env->n_philo)
	{
		pthread_mutex_lock(&env->nom_mtx);
		if (env->hungry != -1 || env->philos[i].ate < env->hungry)
		{
			alive = u_good(&env->philos[i]);
			if (!alive)
				return (print_death(&env->philos[i]));
		}
		else
			full++;
		pthread_mutex_unlock(&env->nom_mtx);
		i = (i + 1) % env->n_philo;
	}
}



void	sim(t_env *env)
{
	t_id	threads[P_MAX];
	int		i;

	i = 0;
	env->times.start = get_time();
	while (i < env->n_philo)
	{
		pthread_create(&threads[i], NULL, routine, &env->philos[i]);
		i++;
	}
	death_checker(env);
	i = 0;
	while (i < env->n_philo)
	{
		pthread_join(threads[i], NULL);
		i++;
	}
	return ;
}
