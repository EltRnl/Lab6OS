#include <stdio.h>
#include <unistd.h>

#include "tasks_implem.h"
#include "tasks_queue.h"
#include "tasks.h"
#include "debug.h"

tasks_queue_t **tqueue;

pthread_t **thread_pool;

pthread_cond_t empty_queue;

__thread long thread_index;

int nb_exec;

void create_queues(void)
{
    tqueue = malloc(THREAD_COUNT*sizeof(tasks_queue_t*));
    for (unsigned int i = 0; i < THREAD_COUNT; i++){
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
    thread_index = (long)p;
    PRINT_DEBUG(10, "Thread with index %ld has been created\n",thread_index);
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
    nb_exec = 0;
    thread_index = 0;
    for(long i=0; i<THREAD_COUNT; i++){
        thread_pool[i] = malloc(sizeof(pthread_t));
        long k = i;
        pthread_create(thread_pool[i], NULL,&worker_thread, (void *)k);
    }
    
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

int get_nb_exec(){
    return nb_exec;
}

int get_queue_size(tasks_queue_t *q){
    return q->index - q->start;
}

unsigned int get_all_queue_sizes(){
    unsigned int sum=0;
    for(int i=0;i<THREAD_COUNT;i++){
        sum+=get_queue_size(tqueue[i]);
    }
    return sum;
}

void resize_queue(tasks_queue_t *q){
    if(q->start>0){
        PRINT_DEBUG(100, "Giving back the stolen space to queue #%ld\n", thread_index);
        unsigned int delta = q->start;
        q->start = 0;
        q->index -= delta;
        for(int i=0; i<q->index;i++){
            q->task_buffer[i]=q->task_buffer[i+delta]; 
        }
    }
    if((float)q->index>0.9*((float)q->task_buffer_size)){
        PRINT_DEBUG(100, "Resizing queue #%ld : %u -> %u\n", thread_index,  tqueue[thread_index]->task_buffer_size, tqueue[thread_index]->task_buffer_size*2);
        q->task_buffer = realloc(q->task_buffer, 2*q->task_buffer_size*sizeof(task_t*));
        q->task_buffer_size*=2;
    }    
}

void dispatch_task(task_t *t)
{
    pthread_mutex_lock(&mutex);
    if(tqueue[thread_index]->index==tqueue[thread_index]->task_buffer_size){
        resize_queue(tqueue[thread_index]);
    }
    enqueue_task(tqueue[thread_index], t);
    thread_index = (thread_index + 1)%THREAD_COUNT;
    pthread_mutex_unlock(&mutex);
    pthread_cond_broadcast(&empty_queue); 
}

task_t* steal_task(){
    for (int i = (thread_index+1)%THREAD_COUNT; i !=thread_index; i=(i+1)%THREAD_COUNT){
        if(get_queue_size(tqueue[i])>0){  
            return dequeue_first(tqueue[i]);
        }
    }
    return NULL;
}

task_t* get_task_to_execute(void)
{
    pthread_mutex_lock(&mutex);
    task_t* t = NULL;
    while(get_queue_size(tqueue[thread_index])<=0){
        if((t = steal_task())!=NULL) break;
        pthread_cond_wait(&empty_queue, &mutex);   
    }    
    if(t==NULL) t = dequeue_task(tqueue[thread_index]);
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
