#include <stdio.h>
#include <unistd.h>

#include "tasks_implem.h"
#include "tasks_queue.h"
#include "tasks.h"
#include "debug.h"

tasks_queue_t **tqueue= NULL;

pthread_t **thread_pool;

pthread_cond_t empty_queue;

__thread int thread_index;


int nb_exec;

void create_queues(void)
{
    tqueue = malloc(THREAD_COUNT*sizeof(tasks_queue_t));
    for (int i = 0; i < THREAD_COUNT; i++){
        tqueue[i]=create_tasks_queue();
    }
}

void delete_queues(void)
{
    for (int i = 0; i < THREAD_COUNT; i++){
        free_tasks_queue(tqueue[i]);
    }
    free(tqueue);
}  

void * worker_thread(void * p){
    thread_index = (int)p;
    while(1){
        active_task = get_task_to_execute();

        task_return_value_t ret = exec_task(active_task);
        PRINT_DEBUG(100, "Task %u has finished step %d\n", active_task->task_id,active_task->step);
        if (ret == TASK_COMPLETED){
            terminate_task(active_task);
        }
    #ifdef WITH_DEPENDENCIES
        else{
            PRINT_DEBUG(100, "Task %u is waiting for dependencies\n", active_task->task_id);            
            pthread_mutex_lock(&(active_task->children_lock));
            active_task->status = WAITING;
            if(active_task->task_dependency_done == active_task->task_dependency_count){
                active_task->status = READY;
                dispatch_task(active_task);
            }
            pthread_mutex_unlock(&(active_task->children_lock));
        }
    #endif
        pthread_mutex_lock(&mutex);
        __atomic_fetch_sub(&nb_exec,1,__ATOMIC_SEQ_CST);
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&wait);
    }    
}

void create_thread_pool(void)
{
    thread_pool = malloc(THREAD_COUNT*sizeof(pthread_t*));

    for(int i=0; i<THREAD_COUNT; i++){
        thread_pool[i] = malloc(sizeof(pthread_t));
        int k = i;
        pthread_create(thread_pool[i], NULL,&worker_thread, &k);
    }
    nb_exec = 0;
    pthread_cond_init(&empty_queue,NULL);
    pthread_mutex_init(&mutex,NULL);
    pthread_cond_init(&wait,NULL);
}

void delete_thread_pool(void)
{
    for(int i=0; i<THREAD_COUNT; i++){  
        pthread_cancel(*thread_pool[i]);
    }
}

int get_queue_size(){
    return tqueue[thread_index]->index;
}

int get_all_queue_sizes(){
    int sum=0;
    for(int i=0,i<THREAD_COUNT;i++){
        sum+=tqueue[i]->index;
    }
    return sum;
}

int get_nb_exec(){
    return nb_exec;
}

void dispatch_task(task_t *t)
{
    pthread_mutex_lock(&mutex);
    if(get_queue_size()==tqueue[thread_index]->task_buffer_size){
        tqueue[thread_index]->task_buffer = realloc(tqueue[thread_index]->task_buffer, 2*tqueue[thread_index]->task_buffer_size*sizeof(task_t*));
        tqueue[thread_index]->task_buffer_size*=2;
        PRINT_DEBUG(100, "Resizing queue %u -> %u\n", tqueue->task_buffer_size/2, tqueue->task_buffer_size);
    }
    enqueue_task(tqueue[thread_index], t);
    
    pthread_mutex_unlock(&mutex);
    //pthread_cond_signal(&empty_queue); 
}

task_t* get_task_to_execute(void)
{
    pthread_mutex_lock(&mutex);
    while(get_queue_size()<=0){
        
        pthread_mutex_unlock(&mutex);
        usleep(10);                     //TODO La technique du shlag! à changer plus tard
        pthread_mutex_lock(&mutex);
        /*
        pthread_cond_wait(&empty_queue, &mutex); 
        */
    }
    task_t* t = dequeue_task(tqueue);
    __atomic_fetch_add(&nb_exec,1,__ATOMIC_SEQ_CST);
 
    pthread_mutex_unlock(&mutex);

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
    t->status = TERMINATED;
#ifdef WITH_DEPENDENCIES
    if(t->parent_task != NULL){
        task_t *waiting_task = t->parent_task;
        pthread_mutex_lock(&(waiting_task->children_lock));
        waiting_task->task_dependency_done++;
        task_check_runnable(waiting_task);
        pthread_mutex_unlock(&(waiting_task->children_lock));        
    }
#endif
    //__atomic_fetch_sub(&nb_exec,1,__ATOMIC_SEQ_CST);
    PRINT_DEBUG(10, "Termination of task %u (step %u)\n", t->task_id, t->step);
    pthread_cond_signal(&wait);
}

void task_check_runnable(task_t *t)
{
#ifdef WITH_DEPENDENCIES
    if(t->status == WAITING && t->task_dependency_done == t->task_dependency_count){
        t->status = READY;
        dispatch_task(t);
    }
#endif
}
