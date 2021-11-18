#include <stdio.h>

#include "tasks_implem.h"
#include "tasks_queue.h"
#include "tasks.h"
#include "debug.h"

tasks_queue_t *tqueue= NULL;

pthread_t **thread_pool;

pthread_cond_t empty_queue;
pthread_mutex_t mut_queue;

int nb_exec;

void create_queues(void)
{
    tqueue = create_tasks_queue();
}

void delete_queues(void)
{
    free_tasks_queue(tqueue);
}  

void * worker_thread(void * p){
    while(1){
        active_task = get_task_to_execute();

        task_return_value_t ret = exec_task(active_task);
        __atomic_fetch_sub(&nb_exec,1,__ATOMIC_SEQ_CST);
        if (ret == TASK_COMPLETED){
            terminate_task(active_task);
        }
    #ifdef WITH_DEPENDENCIES
        else{
            active_task->status = WAITING;
        }
    #endif
    }    
}

void create_thread_pool(void)
{
    thread_pool = malloc(THREAD_COUNT*sizeof(pthread_t*));

    for(int i=0; i<THREAD_COUNT; i++){
        thread_pool[i] = malloc(sizeof(pthread_t));
        pthread_create(thread_pool[i], NULL,&worker_thread, NULL);
    }
    nb_exec = 0;
    pthread_cond_init(&empty_queue,NULL);
    pthread_mutex_init(&mut_queue,NULL);
    pthread_cond_init(&wait,NULL);
}

void delete_thread_pool(void)
{
    for(int i=0; i<THREAD_COUNT; i++){ 
        pthread_kill(*thread_pool[i],SIGTERM);
    }
}

int get_queue_size(){
    return tqueue->index;
}

int get_nb_exec(){
    return nb_exec;
}

void dispatch_task(task_t *t)
{
    pthread_mutex_lock(&mut_queue);
    if(get_queue_size()==tqueue->task_buffer_size){
        tqueue->task_buffer = realloc(tqueue->task_buffer, 2*tqueue->task_buffer_size*sizeof(task_t*));
        tqueue->task_buffer_size*=2;
        PRINT_DEBUG(100, "Resizing queue %u -> %u\n", tqueue->task_buffer_size/2, tqueue->task_buffer_size);
    }
    enqueue_task(tqueue, t);
    
    pthread_cond_signal(&empty_queue);
    pthread_mutex_unlock(&mut_queue);
}

task_t* get_task_to_execute(void)
{
    pthread_mutex_lock(&mut_queue);
    
    while(get_queue_size()<=0){
        pthread_cond_wait(&empty_queue, &mut_queue);
    }
    __atomic_fetch_add(&nb_exec,1,__ATOMIC_SEQ_CST);
    task_t* t = dequeue_task(tqueue);
 
    pthread_mutex_unlock(&mut_queue);

    return t;
}

unsigned int exec_task(task_t *t)
{
    t->step++;
    t->status = RUNNING;

    PRINT_DEBUG(10, "Execution of task %u (step %u)\n", t->task_id, t->step);
    
    unsigned int result = t->fct(t, t->step);
    return result;
}

void terminate_task(task_t *t)
{
    pthread_mutex_lock(&mut_wait);
    t->status = TERMINATED;
    
    PRINT_DEBUG(10, "Task terminated: %u\n", t->task_id);

#ifdef WITH_DEPENDENCIES
    if(t->parent_task != NULL){
        task_t *waiting_task = t->parent_task;
        waiting_task->task_dependency_done++;
        
        task_check_runnable(waiting_task);
    }
#endif
    //__atomic_fetch_sub(&nb_exec,1,__ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&mut_wait);
    pthread_cond_signal(&wait);
}

void task_check_runnable(task_t *t)
{
#ifdef WITH_DEPENDENCIES
    if(t->task_dependency_done == t->task_dependency_count){
        t->status = READY;
        dispatch_task(t);
    }
#endif
}
