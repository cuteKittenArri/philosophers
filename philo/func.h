/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   func.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: stmuller <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/17 01:39:41 by stmuller          #+#    #+#             */
/*   Updated: 2026/09/17 01:39:43 by stmuller         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FUNC_H
# define FUNC_H

# include "philomilo.h"
# include <unistd.h>
# include <stdio.h>
# include <sys/time.h>

int		ende(char *e_msg);
long	philotoi(char *str);
bool	is_legal(char *str);
int		destroyer(t_env *env, char *e_msg, int counter);
int		init_env(t_env *env, t_mutex *knifes, t_philo *philos);
int		init_knifes(t_env *env, t_mutex *knifes);
void	init_philos(t_env *env, t_philo *philos, t_mutex *knifes);
size_t	get_time(void);
void	eepy(size_t ms);
bool	died(t_env *env);
void	printer(t_philo *philo, char *msg);
void	*routine(void *me);
void	print_death(t_philo *philo);
void	sim(t_env *env);

#endif
