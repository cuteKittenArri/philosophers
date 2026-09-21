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

int	init_env(t_env *env, t_mutex *knifes, t_philo *philos)
{
	env->knifes = knifes;
	env->philos = philos;
	if (pthread_mutex_init(&env->nom_mtx, NULL) != 0
	     || pthread_mutex_init(&env->print_mtx, NULL) != 0)
		return (destroyer(env, "Mutex init failed(env)", -1));
	return (0);
}

int	init_knifes(t_env *env, t_mutex *knifes)
{
	int	i;

	i = 0;
	while (i < env->n_philo)
	{
		if (pthread_mutex_init(&knifes[i], NULL) != 0)
			return (destroyer(env, "Mutex init failed(knifes)", i));
		i++;
	}
	return (0);
}
