#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include "so_scheduler.h"
#include "utils.h"

node_q *create_node(thread *data) {
	node_q *node = malloc(sizeof(node_q));

	node->data = data;
	node->next = NULL;

	return node;
}

void enqueue(queue *q, thread *data) {
	node_q *n = create_node(data);

	if (q->front == NULL) {
		q->front = q->rear = n;
	} else {
		q->rear->next = n;
		q->rear = n;
		q->rear->next = q->front;
	}
	q->size++;
}

void enqueue_front(queue *q, thread *data) {
	node_q *n = create_node(data);

	if (q->front == NULL)
		q->front = q->rear = n;
	else {
		n->next = q->front;
		q->front = n;
		q->rear->next = q->front;
	}
	q->size++;
}

thread *dequeue(queue *q) {
	if (q->front == NULL) //empty queue
		return NULL;

	node_q *n = q->front;

	if (q->front == q->rear) { //queue only has 1 node => rear = NULL
		q->rear = NULL;
		q->front = NULL;
	} else {
		q->front = q->front->next;
	}

	thread *for_return = n->data;

	free(n);
	n = NULL;

	q->size--;

	return for_return;
}

void destroy_queue(queue *q) {
	while (q->size != 0) {
		thread *data = dequeue(q);

		free(data);
		data = NULL;
	}

	free(q);
	q = NULL;
}

queue *init_queue(void) {
	queue *q = malloc(sizeof(queue));

	q->front = NULL;
	q->rear = NULL;
	q->size = 0;

	return q;
}

scheduler *sched = NULL;

thread *get_thread_big_prio(void) {
	for (int i = SO_MAX_PRIO; i >= 0; --i) {
		if (sched->ready_threads[i]->size != 0) {
			thread *node = dequeue(sched->ready_threads[i]);

			if (node != NULL)
				return  node;
		}
	}

	return NULL;
}

void round_robin(int cond, thread *next_thread, thread *thread_info) {
	if (cond) {
		enqueue(sched->ready_threads[sched->curr_thread->priority],
			sched->curr_thread);
		sched->curr_thread = next_thread;
	} else {
		enqueue_front(sched->ready_threads[next_thread->priority],
			next_thread);
	}
	sem_post(&sched->curr_thread->semaphore);
	sem_wait(&thread_info->semaphore);
}

void reschedule(void) {
	thread *thread_info = pthread_getspecific(*sched->key_thread);
	thread *next_thread = get_thread_big_prio();

	thread_info->time_quantum--;
	if (thread_info->time_quantum > 0 && next_thread != NULL) {
		round_robin(
			sched->curr_thread->priority < next_thread->priority,
			next_thread, thread_info);
	} else if (thread_info->time_quantum == 0 && next_thread != NULL) {
		thread_info->time_quantum = sched->time_quantum;
		round_robin(
			sched->curr_thread->priority <= next_thread->priority,
			next_thread, thread_info);
	}
}

void *start_thread(void *arg) {
	arguments *args = (arguments *)arg;
	unsigned int priority = args->priority;
	thread *th = args->thread_info;
	so_handler *func = args->handler;

	free(args);

	pthread_setspecific(*sched->key_thread, th);

	sem_wait(&th->semaphore);

	func(priority);

	sched->curr_thread = get_thread_big_prio();

	enqueue_front(sched->terminated_threads, th);

	if (sched->curr_thread != NULL)
		sem_post(&sched->curr_thread->semaphore);
	else
		sem_post(&sched->prev_thread->semaphore);

	return NULL;
}

int so_init(unsigned int time_quantum, unsigned int io) {
	if (sched != NULL)
		return -1;

	if (io > SO_MAX_NUM_EVENTS || time_quantum == 0)
		return -1;

	sched = malloc(sizeof(scheduler));
	if (sched == NULL)
		return -1;

	sched->time_quantum = time_quantum;
	sched->io = io;

	sched->terminated_threads = init_queue();
	if (sched->terminated_threads == NULL) {
		free(sched);
		sched = NULL;
		return -1;
	}

	sched->ready_threads = calloc(SO_MAX_PRIO + 1, sizeof(queue *));
	if (sched->ready_threads == NULL) {
		free(sched);
		sched = NULL;
		return -1;
	}

	for (int i = 0; i < SO_MAX_PRIO + 1; ++i)
		sched->ready_threads[i] = init_queue();

	sched->blocked_threads = calloc(io, sizeof(thread *));
	if (sched->blocked_threads == NULL) {
		free(sched);
		sched = NULL;
		return -1;
	}

	for (int i = 0; i < io; ++i)
		sched->blocked_threads[i] = init_queue();

	sched->prev_thread = NULL;

	sched->curr_thread = malloc(sizeof(thread));
	if (sched->curr_thread == NULL) {
		free(sched);
		sched = NULL;
		return -1;
	}

	sched->curr_thread->thread_id = pthread_self();
	sched->curr_thread->time_quantum = sched->time_quantum;
	sched->curr_thread->priority = 0;
	sem_init(&sched->curr_thread->semaphore, 0, 0);

	sched->key_thread = malloc(sizeof(pthread_key_t));
	pthread_key_create(sched->key_thread, NULL);
	pthread_setspecific(*sched->key_thread, sched->curr_thread);

	reschedule();

	return 0;
}

tid_t so_fork(so_handler *func, unsigned int priority) {
	if (func == NULL || priority > SO_MAX_PRIO)
		return INVALID_TID;

	thread *thread_info = pthread_getspecific(*sched->key_thread);
	thread *new_thread = malloc(sizeof(thread));

	if (new_thread == NULL)
		return INVALID_TID;

	new_thread->priority = priority;
	new_thread->time_quantum = sched->time_quantum;

	arguments *args = malloc(sizeof(arguments));

	args->priority = priority;
	args->thread_info = new_thread;
	args->handler = func;

	sem_post(&thread_info->semaphore);

	sem_init(&new_thread->semaphore, 0, 1);
	sem_wait(&new_thread->semaphore);

	pthread_create(&new_thread->thread_id, NULL, start_thread, args);

	enqueue(sched->ready_threads[new_thread->priority], new_thread);

	sem_wait(&thread_info->semaphore);

	reschedule();

	return new_thread->thread_id;
}

int so_wait(unsigned int io) {
	if (io < 0 || io >= sched->io)
		return -1;

	thread *thread_info = pthread_getspecific(*sched->key_thread);

	sem_post(&thread_info->semaphore);
	sem_wait(&thread_info->semaphore);

	sched->curr_thread = get_thread_big_prio();

	enqueue(sched->blocked_threads[io], thread_info);

	sem_post(&sched->curr_thread->semaphore);
	sem_wait(&thread_info->semaphore);

	return 0;
}

int so_signal(unsigned int io) {
	if (io < 0 || io >= sched->io)
		return -1;

	int count = 0;
	thread *thread_info = pthread_getspecific(*sched->key_thread);

	sem_post(&thread_info->semaphore);

	while (sched->blocked_threads[io]->size != 0) {
		thread *curr = dequeue(sched->blocked_threads[io]);

		enqueue(sched->ready_threads[curr->priority], curr);
		count++;
	}

	sem_wait(&thread_info->semaphore);

	reschedule();

	return count;
}

void so_exec(void) {
	thread *thread_info = pthread_getspecific(*sched->key_thread);

	sem_post(&thread_info->semaphore);
	sem_wait(&thread_info->semaphore);

	reschedule();
}

void so_end(void) {
	if (sched == NULL)
		return;
	thread *thread_info = pthread_getspecific(*sched->key_thread);

	sched->prev_thread = thread_info;

	sched->curr_thread = get_thread_big_prio();
	if (sched->curr_thread != NULL) {
		sem_post(&sched->curr_thread->semaphore);
		sem_wait(&thread_info->semaphore);
	}

	while (sched->terminated_threads->size != 0) {
		thread *curr = dequeue(sched->terminated_threads);

		pthread_join(curr->thread_id, NULL);
		sem_destroy(&curr->semaphore);
		free(curr);
		curr = NULL;
	}

	sem_destroy(&thread_info->semaphore);
	free(thread_info);

	pthread_key_delete(*sched->key_thread);
	free(sched->key_thread);

	for (int i = 0; i < SO_MAX_PRIO + 1; ++i)
		destroy_queue(sched->ready_threads[i]);

	free(sched->ready_threads);

	for (int i = 0; i < sched->io; ++i)
		destroy_queue(sched->blocked_threads[i]);

	free(sched->blocked_threads);

	destroy_queue(sched->terminated_threads);

	free(sched);
	sched = NULL;
}
