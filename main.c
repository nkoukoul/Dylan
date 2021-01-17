#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "strand.h"

pthread_mutex_t event_lock;

struct thread_info{
    struct strand * sd_;
    int thread_num_;
};

void func(int job_id, struct strand * sd){
    //printf("job number %d\n", job_id);
    void (*func_ptr)(int, struct strand *sd) = func;
    if (job_id < 1000)
    {
        add_job(sd, func_ptr);
    }
}

void * executor(void * thread_info)
{
    struct thread_info * tf = (struct thread_info *) thread_info;
    int thread_id = tf->thread_num_;
    void (*func_ptr)(int, struct strand *sd) = func;
    add_job(tf->sd_, func_ptr);
    while (1)
    {
        struct job * jb = get_job(tf->sd_);
        if (jb)
        {
            printf("hello from thread %d ", thread_id);
            jb->func_ptr_(jb->job_id_, tf->sd_);
            free(jb);
        }
        else
        {
            break;
        }
    }    
}

int main(int argc, char *argv[])
{
    char *ptr;
    long thread_number = strtol(argv[1], &ptr, 10);
    struct strand * sd = create_strand();
    pthread_t * thread_arr = (pthread_t *) malloc(thread_number * sizeof(pthread_t));
    for (long i = 0; i < thread_number; i++)
    {
        struct thread_info * tf = (struct thread_info *)malloc(sizeof(struct thread_info));
        tf->sd_ = sd;
        tf->thread_num_ = (int) i;
        pthread_t server_thread;
        int s = pthread_create(&server_thread, NULL, executor, (void *)tf);
        if (s != 0)
        {
            perror("thread creation failed.. exiting\n");
            exit(EXIT_FAILURE);
        }
        printf("creating thread %ld\n", i);
        thread_arr[i] = server_thread;
    }
    for (long i = 0; i < thread_number; i++)
    {
        void * res;
        int s = pthread_join(thread_arr[i], &res);
        if (s != 0)
        {
            perror("thread join failed.. exiting\n");
            exit(EXIT_FAILURE);
        }
    }
    pthread_mutex_destroy(&event_lock);
    return 0;
}