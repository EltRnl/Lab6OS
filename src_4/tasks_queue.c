#include <stdio.h>
#include <stdlib.h>

#include "tasks_queue.h"


tasks_queue_t* create_tasks_queue(void)
{
    tasks_queue_t *q = (tasks_queue_t*) malloc(sizeof(tasks_queue_t));

    q->task_buffer_size = QUEUE_SIZE;
    q->task_buffer = (task_t**) malloc(sizeof(task_t*) * q->task_buffer_size);

    q->index = 0;
    q->start = 0;

    return q;
}


void free_tasks_queue(tasks_queue_t *q)
{
    /* IMPORTANT: We chose not to free the queues to simplify the
     * termination of the program (and make debugging less complex) */
    
    /* free(q->task_buffer); */
    /* free(q); */
}

void enqueue_task(tasks_queue_t *q, task_t *t)
{
    if(q->index == q->task_buffer_size){
        fprintf(stderr,"ERROR: the queue of tasks is full\n");
        exit(EXIT_FAILURE);
    }

    q->task_buffer[q->index] = t;
    q->index++;
}


task_t* dequeue_task(tasks_queue_t *q)
{
    if(q->index == 0){
        return NULL;
    }

    task_t *t = q->task_buffer[q->index-1];
    q->index--;

    return t;
}

task_t* dequeue_first(tasks_queue_t *q)
{
    task_t *t = q->task_buffer[q->start];
    q->start++;
    return t;
}

