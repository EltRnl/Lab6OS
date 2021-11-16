#ifndef __TASKS_IMPLEM_H__
#define __TASKS_IMPLEM_H__

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include "tasks_types.h"

void create_queues(void);
void delete_queues(void);

void create_thread_pool(void);
void delete_thread_pool(void);

void dispatch_task(task_t *t);
task_t* get_task_to_execute(void);
unsigned int exec_task(task_t *t);
void terminate_task(task_t *t);
int get_queue_size(void);

void task_check_runnable(task_t *t);

#endif
