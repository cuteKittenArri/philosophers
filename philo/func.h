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
# include <stdlib.h>
# include <stdbool.h>

void	ende(char *e_msg);
int		philotoi(char *str);
bool	is_legal(char *str);
void	destroyer(t_env *env, char *e_msg, int counter);
void	init_env(t_env *env, t_mutex *knifes, t_philo *philos);
void	init_knifes(t_env *env, t_mutex *knifes, int counter);

#endif
