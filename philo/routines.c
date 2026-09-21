#include "func.h"
#include "philomilo.h"
#include <time.h>

void	grab_em(t_philo *philo)
{
	if (philo->l_mtx < philo->r_mtx)
	{
		pthread_mutex_lock(philo->l_mtx);
		printer(philo, "has taken a knife(scary)");
		pthread_mutex_lock(philo->r_mtx);
		printer(philo, "has taken a knife(scary)");
	}
	else
	{
		pthread_mutex_lock(philo->r_mtx);
		printer(philo, "has taken a knife(scary)");
		pthread_mutex_lock(philo->l_mtx);
		printer(philo, "has taken a knife(scary)");
	}
}

void	mahlzeit(t_philo *philo)
{
	grab_em(philo);
	pthread_mutex_lock(&philo->env->nom_mtx);
	philo->last_nom = get_time();
	philo->ate++;
	pthread_mutex_unlock(&philo->env->nom_mtx);
	printer(philo, "is eating");
	eepy(philo->times.nom);
	pthread_mutex_unlock(philo->l_mtx);
	pthread_mutex_unlock(philo->r_mtx);
}

void	*routine(void *me)
{
	t_philo *philo;

	philo = (t_philo *)me;
	
	if (philo->id % 2 == 1)
		eepy(50);
	while (!died(philo->env))
	{
		mahlzeit(philo);
		if (philo->ate >= philo->env->hungry)
			break ;
		printer(philo, "is sleeping");
		eepy(philo->times.sleep);
		printer(philo, "is thinking");
	}
	return (NULL);
}
