/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   philomilo.h                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: stmuller <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 01:49:29 by stmuller          #+#    #+#             */
/*   Updated: 2026/09/16 01:49:59 by stmuller         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef PHILOMILO_H
# define PHILOMILO_H

#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

# define P_MAX 200

typedef pthread_t		t_id;
typedef pthread_mutex_t	t_mutex;
typedef unsigned char	t_philo_id;

typedef struct	s_times
{
	size_t	start;
	size_t	die;
	size_t	sleep;
	size_t	meal;
	size_t	last_meal;
}	t_times;

typedef struct	s_mutexehexe
{
	t_mutex	*print_mtx;
	t_mutex	*nom_mtx;
	t_mutex	*l_mtx;
	t_mutex	*r_mtx;
}	t_muthexe;

typedef struct s_philo
{
	t_philo_id	id;
	t_muthexe	muthexe;
	t_times		times;
	int			hungry;
}	t_philo;

typedef struct	s_env
{
	t_mutex	print_mtx;
	t_mutex	nom_mtx;
	t_mutex	*knife;
	t_philo	*philos;
	int		philo_count;
}	t_env;

#endif
