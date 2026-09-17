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

void	init_env(t_env *env, t_mutex *knifes, t_philo *philos)
{
	env->knifes = knifes;
	env->philos = philos;
	if (pthread_mutex_init(&env->nom_mtx, NULL) != 0
	     || pthread_mutex_init(&env->print_mtx, NULL) != 0)
		destroyer(env, "Mutex init failed(env)", -1);
}

void	init_knifes(t_env *env, t_mutex *knifes, int counter)
{
	int	i;

	i = 0;
	while (i < counter)
	{
		if (pthread_mutex_init(&knifes[i], NULL) != 0)
			destroyer(env, "Mutex init failed(knifes)", i);
		i++;
	}
}
