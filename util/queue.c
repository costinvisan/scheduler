#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

queue* init_queue() {
	queue* q = malloc(sizeof(queue));
    if (q == NULL)
        return NULL;

	q->front = NULL;
	q->rear = NULL;
	q->size = 0;

	return q;
}

node_q* create_node(thread *data) {
	node_q* node = malloc(sizeof(node_q));
	node->data = data;
	node->prev = NULL;

	return node;
}

void enqueue(queue* q, thread *data) {
	node_q* n = create_node(data);
	if(q->front == NULL)
		q->front = q->rear = n;
	else{
		q->rear->next = n;
		q->rear = n;
		q->rear->next = q->front;
	}
	q->size++;
}

void enqueue_first(queue *q, thread *data) {
	node_q* n = create_node(data);
	if(q->front == NULL)
		q->front = q->rear = n;
	else{
		n->next = q->front;
		q->front = n;
		q->rear->next = q->front;
	}
	q->size++;
}

node_q* dequeue(queue* q) {
	if(q->front == NULL) //empty queue
		return NULL;

	node_q* n = q->front;
	if(q->front == q->rear) { //queue only has 1 node => rear = NULL
		q->rear = NULL;
		q->front = NULL;
	} else {
		q->front = q->front->next;
	}
	
	free(n);
	n = NULL;

	q->size--;

	return n;
}

void destroy_queue(queue *q) {
	while (q->size != 0) {
		dequeue(q);
	}

	free(q);
	q = NULL;
}
