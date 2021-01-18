#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "strand.h"
#include "io_context.h"

pthread_mutex_t event_lock;

struct thread_info
{
    struct strand *sd_;
    int thread_num_;
};

void *executor(void *thread_info)
{
    struct thread_info *tf = (struct thread_info *)thread_info;
    int thread_id = tf->thread_num_;
    void (*func_ptr)(struct strand *, struct client_context *) = handle_connections;
    add_job(tf->sd_, NULL, func_ptr);
    while (1)
    {
        struct job *jb = get_job(tf->sd_);
        if (jb)
        {
            //printf("hello from thread %d ", thread_id);
            jb->func_ptr_(tf->sd_, jb->c_ctx_);
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
    char *ipaddr = argv[1];
    int port = (int)strtol(argv[2], &ptr, 10);
    int thread_number = (int)strtol(argv[3], &ptr, 10);
    struct io_context *ioc = initialize_io_context(ipaddr, port);
    struct strand *sd = create_strand(ioc);
    pthread_t *thread_arr = (pthread_t *)malloc(thread_number * sizeof(pthread_t));
    for (int i = 0; i < thread_number; i++)
    {
        struct thread_info *tf = (struct thread_info *)malloc(sizeof(struct thread_info));
        tf->sd_ = sd;
        tf->thread_num_ = (int)i;
        pthread_t server_thread;
        int s = pthread_create(&server_thread, NULL, executor, (void *)tf);
        if (s != 0)
        {
            perror("thread creation failed.. exiting\n");
            exit(EXIT_FAILURE);
        }
        printf("creating thread %d\n", i);
        thread_arr[i] = server_thread;
    }
    for (long i = 0; i < thread_number; i++)
    {
        void *res;
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