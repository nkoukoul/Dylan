#ifndef STRAND_H
#define STRAND_H

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

struct strand;
struct client_context;

struct job
{
    struct job *next_;
    void (*func_ptr_)(struct strand *, struct client_context *);
    struct client_context *c_ctx_;
    int job_id_;
};

void clear_job(struct job* jb)
{
    jb->next_ = NULL;
    jb->c_ctx_ = NULL;//client context will be free when request is complete
}

struct strand
{
    struct job *cursor_;
    pthread_mutex_t strand_lock_;
    int size_;
    int job_id_;
    struct server_context *ioc_;
};

pthread_mutex_t initialize_lock(struct strand *sd)
{
    pthread_mutex_t mut;
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) == -1)
    {
        perror("mutexattr_init error");
        exit(1);
    }

    if (pthread_mutex_init(&mut, &attr) == -1)
    {
        perror("mutex_init error");
        exit(2);
    }
    return mut;
}

struct strand *create_strand(struct server_context *ioc)
{
    struct strand *sd;
    sd = (struct strand *)malloc(sizeof(struct strand));
    sd->cursor_ = NULL;
    sd->size_ = 0;
    sd->job_id_ = 0;
    sd->ioc_ = ioc;
    sd->strand_lock_ = initialize_lock(sd);
    return sd;
}

struct job *front(struct strand *sd)
{
    return sd->cursor_->next_;
}

struct job *back(struct strand *sd)
{
    return sd->cursor_;
}

void add_job(struct strand *sd, struct client_context *c_ctx, void (*func)(struct strand *, struct client_context *))
{
    struct job *new_job;
    new_job = (struct job *)malloc(sizeof(struct job));
    new_job->func_ptr_ = func;
    new_job->c_ctx_ = c_ctx;
    pthread_mutex_lock(&sd->strand_lock_);
    if (sd->cursor_) //queue not empty
    {
        new_job->next_ = sd->cursor_->next_;
        sd->cursor_->next_ = new_job;
    }
    else //queue empty
    {
        new_job->next_ = new_job;
        sd->cursor_ = new_job;
    }
    //advance cursor
    sd->cursor_ = sd->cursor_->next_;
    sd->size_++;
    sd->job_id_++;
    new_job->job_id_ = sd->job_id_;
    pthread_mutex_unlock(&sd->strand_lock_);
}

struct job *get_job(struct strand *sd)
{
    pthread_mutex_lock(&sd->strand_lock_);
    if (!sd->size_)
    {
        pthread_mutex_unlock(&sd->strand_lock_);
        return NULL;
    }
    struct job *first_job = front(sd);
    if (first_job == sd->cursor_)
    {
        sd->cursor_ = NULL;
    }
    else
    {
        sd->cursor_->next_ = first_job->next_;
    }
    sd->size_--;
    pthread_mutex_unlock(&sd->strand_lock_);
    return first_job;
}

#endif //STRAND_H