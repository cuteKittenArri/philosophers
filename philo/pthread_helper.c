/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   pthread_helper.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: stmuller <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/17 02:29:40 by stmuller          #+#    #+#             */
/*   Updated: 2026/09/17 02:29:42 by stmuller         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "func.h"
#include "philomilo.h"
#include <pthread.h>

int	destroyer(t_env *env, char *e_msg, int counter)
{
	while (--counter >= 0)
		pthread_mutex_destroy(&env->knifes[counter]);
	pthread_mutex_destroy(&env->nom_mtx);
	pthread_mutex_destroy(&env->print_mtx);
	pthread_mutex_destroy(&env->died_mtx);
	if (!e_msg)
		return (0);
	return (ende(e_msg));
}

void	printer(t_philo *philo, char *msg)
{
	size_t	now;

	pthread_mutex_lock(&philo->env->print_mtx);
	now = get_time() - philo->times.start;
	if (!died(philo->env))
		printf("%lu %d %s\n", now, philo->id, msg);
	pthread_mutex_unlock(&philo->env->print_mtx);
}

bool	died(t_env *env)
{
	bool	oof;
	pthread_mutex_lock(&env->died_mtx);
	oof = env->died;
	pthread_mutex_unlock(&env->died_mtx);
	return (oof);
}

static void	death(t_env *env)
{
	pthread_mutex_lock(&env->died_mtx);
	env->died = true;
	pthread_mutex_unlock(&env->died_mtx);
}

void	print_death(t_philo *philo)
{
	pthread_mutex_lock(&philo->env->print_mtx);
	printf("%lu %d died\n", get_time() - philo->times.start, philo->id);
	death(philo->env);
	pthread_mutex_unlock(&philo->env->print_mtx);
}
