/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   free.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hoskim <hoskim@student.42prague.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/04 19:31:27 by hoskim            #+#    #+#             */
/*   Updated: 2025/07/05 15:48:09 by hoskim           ###   ########seoul.kr  */
/*                                                                            */
/* ************************************************************************** */

#include "philo.h"

static int	check_for_death(t_simulation *sim)
{
	int			i;
	long long	current_time;
	long long	time_since_last_meal;

	i = 0;
	current_time = get_current_time_ms();
	while (i < sim->philosopher_count)
	{
		time_since_last_meal = current_time - sim->philosophers[i].last_meal_time;
		if (time_since_last_meal >= sim->time_to_die)
		{
			sim->simulation_ended = TRUE;
			return (i + 1);
		}
		i++;
	}
	return (FALSE);
}

static int	check_all_philosophers_satisfied(t_simulation *sim)
{
	int	i;
	int	satisfied_count;

	if (sim->required_meals == -1)
		return (FALSE);
	i = 0;
	satisfied_count = 0;
	while (i < sim->philosopher_count)
	{
		if (sim->philosophers[i].meals_eaten >= sim->required_meals)
			satisfied_count++;
		i++;
	}
	if (satisfied_count >= sim->philosopher_count)
	{
		sim->simulation_ended = TRUE;
		return (TRUE);
	}
	return (FALSE);
}

static int	evaluate_simulation_status(t_simulation *sim)
{
	int	death_philosopher_id;
	int	all_satisfied;

	pthread_mutex_lock(&sim->data_mutex);
	death_philosopher_id = check_for_death(sim);
	if (death_philosopher_id > 0)
	{
		pthread_mutex_unlock(&sim->data_mutex);
		print_philosopher_status(&sim->philosophers[death_philosopher_id - 1], "died", TRUE);
		return (TRUE);
	}
	all_satisfied = check_all_philosophers_satisfied(sim);
	pthread_mutex_unlock(&sim->data_mutex);
	return (all_satisfied);
}

static void	cleanup_simulation_resources(t_simulation *sim)
{
	int	i;

	i = -1;
	while (++i < sim->philosopher_count)
		pthread_join(sim->philosophers[i].thread, NULL);
	i = -1;
	while (++i < sim->philosopher_count)
		pthread_mutex_destroy(&sim->fork_mutexes[i]);
	pthread_mutex_destroy(&sim->print_mutex);
	pthread_mutex_destroy(&sim->data_mutex);
	if (sim->philosophers)
	{
		free(sim->philosophers);
		sim->philosophers = NULL;
	}
	if (sim->fork_mutexes)
	{
		free(sim->fork_mutexes);
		sim->fork_mutexes = NULL;
	}
}

void	monitor_simulation_and_cleanup(t_simulation *sim)
{
	int	check_interval_us;

	check_interval_us = sim->time_to_die / 10;
	if (check_interval_us < 500)
		check_interval_us = 500;
	if (check_interval_us > 5000)
		check_interval_us = 5000;
	while (TRUE)
	{
		if (evaluate_simulation_status(sim) == TRUE)
			break ;
		usleep(check_interval_us);
	}
	cleanup_simulation_resources(sim);
}
