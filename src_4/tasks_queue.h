#ifndef __TASKS_QUEUE_H__
#define __TASKS_QUEUE_H__

#include "tasks.h"


typedef struct tasks_queue{
    task_t** task_buffer;
    unsigned int task_buffer_size;
    unsigned int index;
} tasks_queue_t;

/* TODO Idea for the new tasks_queue structure

typedef struct tasks_list{
    task_t elem;
    struct tasks_list * prev;
    struct tasks_list * next;
} tasks_list_t

typedef struct tasks_queue{
    tasks_list_t* head;
    tasks_list_t* tail; 
    unsigned int size;
} tasks_queue_t;

*/

tasks_queue_t* create_tasks_queue(void);
void free_tasks_queue(tasks_queue_t *q);

void enqueue_task(tasks_queue_t *q, task_t *t);
task_t* dequeue_task(tasks_queue_t *q);
task_t* dequeue_first(tasks_queue_t *q);

#endif
