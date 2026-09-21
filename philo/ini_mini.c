/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ini_mini.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: stmuller <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/17 02:05:05 by stmuller          #+#    #+#             */
/*   Updated: 2026/09/17 02:05:06 by stmuller         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "func.h"
#include <pthread.h>

int	init_env(t_env *env, t_mutex *knifes, t_philo *philos)
{
	env->knifes = knifes;
	env->philos = philos;
	env->died = false;
	if (pthread_mutex_init(&env->nom_mtx, NULL) != 0)
		return (ende("Mutex init failure!(nom_mtx)"));
	if (pthread_mutex_init(&env->print_mtx, NULL) != 0)
	{
		pthread_mutex_destroy(&env->nom_mtx);
		return (ende("Mutex init failure!(print_mtx)"));
	}
	if (pthread_mutex_init(&env->died_mtx, NULL) != 0)
	{
		pthread_mutex_destroy(&env->nom_mtx);
		pthread_mutex_destroy(&env->print_mtx);
		return (ende("Mutex init failure!(died_mtx)"));
	}
	return (0);
}

int	init_knifes(t_env *env, t_mutex *knifes)
{
	int	i;

	i = 0;
	while (i < env->n_philo)
	{
		if (pthread_mutex_init(&knifes[i], NULL) != 0)
			return (destroyer(env, "Mutex init failure!(knifes)", i));
		i++;
	}
	return (0);
}

void	init_philos(t_env *env, t_philo *philos, t_mutex *knifes)
{
	int	i;

	i = -1;
	env->times.start = get_time();
	while (++i < env->n_philo)
	{
		philos[i].id = i + 1;
		philos[i].env = env;
		philos[i].times = env->times;
		philos[i].l_mtx = &knifes[i];
		philos[i].r_mtx = &knifes[(i + 1) % env->n_philo];
		philos[i].ate = 0;
		philos[i].last_nom = philos[i].times.start;
	}
}
