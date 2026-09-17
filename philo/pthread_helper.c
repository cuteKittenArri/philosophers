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

#include "philomilo.h"

void	destroyer(t_env *env, char *e_msg, int counter)
{
	while (--counter >= 0)
		pthread_mutex_destroy(&env->knifes[counter]);
	pthread_mutex_destroy(&env->nom_mtx);
	pthread_mutex_destroy(&env->print_mtx);
}
