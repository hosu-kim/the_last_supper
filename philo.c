/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   philo.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hoskim <hoskim@student.42prague.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/04 19:22:39 by hoskim            #+#    #+#             */
/*   Updated: 2025/07/16 00:10:59 by hoskim           ###   ########seoul.kr  */
/*                                                                            */
/* ************************************************************************** */

#include "philo.h"

/**
 * @brief Makes a philosopher acquire their left and right forks.
 * 
 * To preven deadlock, the order of acquiring forks is different
 * depending on whether the philosopher's ID is odd or even:
 * 
 * 1. Odd-numbered philosophers: pick up the left fork first, then the right.
 * 2. Even-numbered philosophers: pick up the right fork first, then the left.
 * 
 * @param philo The philosopher who is acquiring the forks.
 * @param sim A pointer to the simulation struct containing the mutexes.
 */
static void	acquire_forks(t_philosopher *philo, t_simulation *sim)
{
	if (philo->id % 2 == 1)
	{
		pthread_mutex_lock(&sim->fork_mutexes[philo->left_fork_index]);
		print_timestamp_and_philo_status_msg(philo, "has taken a left fork", NOT_DEAD);
		pthread_mutex_lock(&sim->fork_mutexes[philo->right_fork_index]);
		print_timestamp_and_philo_status_msg(philo, "has taken a right fork", NOT_DEAD);
	}
	else if (philo->id % 2 == 1)
	{
		pthread_mutex_lock(&sim->fork_mutexes[philo->right_fork_index]);
		print_timestamp_and_philo_status_msg(philo, "has taken a right fork", NOT_DEAD);
		pthread_mutex_lock(&sim->fork_mutexes[philo->left_fork_index]);
		print_timestamp_and_philo_status_msg(philo, "has taken a left fork", NOT_DEAD);
	}
	else
	{
		pthread_mutex_lock(&sim->fork_mutexes[philo->right_fork_index]);
		print_timestamp_and_philo_status_msg(philo, "has taken a right fork", NOT_DEAD);
		pthread_mutex_lock(&sim->fork_mutexes[philo->left_fork_index]);
		print_timestamp_and_philo_status_msg(philo, "has taken a left fork", NOT_DEAD);
	}
}

/**
 * @brief Makes a philosopher release their left and right forks.
 * 
 * After a philosopher has finished eating, this function unlocks the mutexes
 * for both the left and right forks, making them available for
 * other philosophers to use.
 * 
 * @param philo The philosopher who is releasing the forks.
 * @param sim A pointer to the simulation structure containing the mutexes.
 */
static void	release_forks(t_philosopher *philo, t_simulation *sim)
{
	pthread_mutex_unlock(&sim->fork_mutexes[philo->left_fork_index]);
	pthread_mutex_unlock(&sim->fork_mutexes[philo->right_fork_index]);
}

/**
 * @brief Makes a philosopher wait for a specified duration
 *        while monitoring the simulation's state.
 * 
 * This function implements a precise delay by looping in short intervals
 * (`usleep`) until the total specified duration has passed.
 * During this time, it continuously checks if the simulation has concluded,
 * ensuring the philosopher can react promptly to the end state.
 * 
 * @param philo Pointer to the philosopher's data structure
 * @param duration_ms The total time to wait, in milliseconds.
 * 
 * @note The function will exit before the full duration has elapsed if the
 *       simulation is detected to have finished.
 * @note It uses `usleep(500)` for fine-grained delay, allowing for responsive
 *       checking of the simulation's status.
 */
static void	philo_spend_time(t_philosopher *philo, long long duration_ms)
{
	long long	start_time;
	long long	current_time;
	long long	remaining_time;

	start_time = get_current_time_ms();
	while (!is_simulation_finished(philo->simulation))
	{
		current_time = get_current_time_ms();
		if (current_time - start_time >= duration_ms)
			break ;
		remaining_time = duration_ms - (current_time - start_time);
		if (remaining_time > 10)
			usleep(1000);
		else if (remaining_time > 1)
			usleep(100);
		else
			usleep(10);
	}
}

/**
 * @brief Handles the philosopher's eating routine.
 * 
 * This function first checks for the edge case of a single philosopher:
 * 
 * 1. Edge case (There's only one philosopher):
 *    The philosopher will take one fork and wait until they starve,
 *    as there's no another fork. This allows the simulation to end correctly.
 * 
 * 2. Normal case:
 *    The philosopher acquires both forks, eats for the specified `time_to_eat`,
 *    and then releases the forks.
 *    It updates the `last_meal_time` and `meals_eaten` count while protecting
 *    the shared data with a `data_mutex` to prevent race conditions.
 *
 * @param philo A pointer to the philosopher who is going to eat.
 */
static void	philosopher_eat(t_philosopher *philo)
{
	t_simulation	*sim;

	sim = philo->simulation;
	if (sim->philosopher_count == 1)
	{
		pthread_mutex_lock(&sim->fork_mutexes[philo->left_fork_index]);
		print_timestamp_and_philo_status_msg(philo, "has taken a fork", NOT_DEAD);
		philo_spend_time(philo, sim->time_to_die + 1);
		pthread_mutex_unlock(&sim->fork_mutexes[philo->left_fork_index]);
		return ;
	}
	acquire_forks(philo, sim);
	print_timestamp_and_philo_status_msg(philo, "is eating", NOT_DEAD);
	pthread_mutex_lock(&sim->data_mutex);
	philo->last_meal_time = get_current_time_ms();
	philo->meals_eaten++;
	pthread_mutex_unlock(&sim->data_mutex);
	philo_spend_time(philo, sim->time_to_eat);
	release_forks(philo, sim);
}

/**
 * @brief Main lifecycle function for a philosopher thread.
 * 
 * Continuously cycles through eating, sleeping, and thinking until the
 * simulation ends. Implements deadlock prevention through staggered starts
 * and livelock prevention for odd philsopher counts.
 * 
 * Key features:
 * 1. Even-numbered philsophers start with a delay to reduce contention
 * 2. Checks simulation status after each major action for prompt terminiation
 * 3. Adds small thinking delay for odd counts to prevent livelock
 * 
 * @param arg Pointer to the philosopher's t_philosopher structure
 * @return NULL on thread completion.
 *         - Thread functions created by pthread_create() must return void *
 */
void	*philosopher_lifecycle(void *arg)
{
	t_philosopher	*philo;
	t_simulation	*sim;

	philo = (t_philosopher *)arg;
	sim = philo->simulation;
	if (philo->id % 2 == 0)
		usleep(sim->time_to_eat / 2);
	while (!is_simulation_finished(sim))
	{
		philosopher_eat(philo);
		if (is_simulation_finished(sim))
			break ;
		print_timestamp_and_philo_status_msg(philo, "is sleeping", NOT_DEAD);
		philo_spend_time(philo, sim->time_to_sleep);
		if (is_simulation_finished(sim))
			break ;
		print_timestamp_and_philo_status_msg(philo, "is thinking", NOT_DEAD);
		if (sim->philosopher_count % 2 == 1)
			usleep(100);
	}
	return (NULL);
}
