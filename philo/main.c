/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: stmuller <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 01:27:48 by stmuller          #+#    #+#             */
/*   Updated: 2026/09/16 01:27:50 by stmuller         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "func.h"
#include "philomilo.h"

int	arg_checker(int argc, char **argv)
{
	int	i;

	i = 0;
	if (argc != 5 && argc != 6)
		return (ende("Wrong argc"));
	while (++i < argc)
	{
		if (!is_legal(argv[i]))
			return (ende("Illegal ARG"));
	}
	return (0);
}

int	parsing(t_env *env, int argc, char **argv)
{
	if (philotoi(argv[1]) > P_MAX || philotoi(argv[1]) < 1)
		return (ende("Invalid Philo amount"));
	env->n_philo = philotoi(argv[1]);
	if (philotoi(argv[2]) <= 0 || philotoi(argv[3]) <= 0 || philotoi(argv[4]) <= 0)
		return (ende("Invalid ARGs"));
	env->times.die = philotoi(argv[2]);
	env->times.nom = philotoi(argv[3]);
	env->times.sleep = philotoi(argv[4]);
	if (argc == 6 && philotoi(argv[5]) > 0)
		env->hungry = philotoi(argv[5]);
	else
		env->hungry = -1;
	return (0);
}

int	main(int argc, char **argv)
{
	t_env		env;
	t_mutex		knifes[P_MAX];
	t_philo		philos[P_MAX];

	if (arg_checker(argc, argv))
		return (1);
	if (parsing(&env, argc, argv))
		return (1);
	if (init_env(&env, knifes, philos))
		return (1);
	if (init_knifes(&env, knifes))
		return (1);
	init_philos(&env, philos, knifes);
	sim(&env);
}
