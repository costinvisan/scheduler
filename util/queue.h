#ifndef QUEUE_H_
#define QUEUE_H_

queue* init_queue();

node_q* create_node(thread *data);

void enqueue(queue *q, thread *data);

void enqueue_first(queue *q, thread *data);

node_q* dequeue(queue *q);

void destroy_queue(queue *q);

#endif /* QUEUE_H_ */