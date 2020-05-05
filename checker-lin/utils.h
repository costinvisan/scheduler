typedef struct thread {
	pthread_t thread_id;
	unsigned int time_quantum;
	unsigned int priority;
	sem_t semaphore;
} thread;

typedef struct node_q {
	thread *data;
	struct node_q *next;
} node_q;

typedef struct queue {
	node_q* front;
	node_q* rear;
	unsigned int size;
} queue;

typedef struct arguments {
	unsigned int priority;
	thread *thread_info;
	so_handler *handler;
} arguments;

typedef struct scheduler {
	unsigned int io;

	unsigned int time_quantum;

	queue **ready_threads;

	queue **blocked_threads;

	queue *terminated_threads;

	thread *curr_thread;

	thread *prev_thread;

	pthread_key_t *key_thread;
} scheduler;
