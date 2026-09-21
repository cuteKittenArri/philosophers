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

# include <pthread.h>
# include <stdbool.h>
# include <stddef.h>

# define P_MAX 200

typedef pthread_t		t_id;
typedef pthread_mutex_t	t_mutex;
typedef unsigned char	t_philo_id;
typedef struct s_env	t_env;

typedef struct s_times
{
	size_t	start;
	size_t	die;
	size_t	nom;
	size_t	sleep;
}	t_times;

typedef struct s_philo
{
	t_philo_id	id;
	t_times		times;
	t_mutex		*r_mtx;
	t_mutex		*l_mtx;
	size_t		last_nom;
	int			ate;
	t_env		*env;
}	t_philo;

typedef struct s_env
{
	t_mutex	print_mtx;
	t_mutex	nom_mtx;
	t_mutex	died_mtx;
	t_mutex	*knifes;
	t_philo	*philos;
	t_times	times;
	int		philo_count;
	int		hungry;
	int		n_philo;
	bool	died;
}	t_env;

#endif
