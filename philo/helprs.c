/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   helprs.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: stmuller <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 01:27:30 by stmuller          #+#    #+#             */
/*   Updated: 2026/09/16 01:27:39 by stmuller         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */
#include "func.h"

bool	is_digit(char c)
{
	return (c >= '0' && '9' >= c);
}

bool	is_legal(char *str)
{
	int	i;

	i = 0;
	if (!str || !*str)
		return (false);
	while (str[i])
	{
		if (!is_digit(str[i]))
			return (false);
		i++;
	}
	return (true);
}

long	philotoi(char *str)
{
	long	ret;

	ret = 0;
	while (*str)
	{
		ret = (ret * 10) + (*str - '0');
		if (ret > 2147483647)
			return (-1);
		str++;
	}
	return (ret);
}

size_t	ft_strlen(char *str)
{
	size_t	i;

	i = 0;
	while (str[i])
		i++;
	return (i);
}

int	ende(char *e_msg)
{
	if (e_msg)
	{
		write(2, e_msg, ft_strlen(e_msg));
		write(2, "\n", 1);
	}
	return (1);
}
