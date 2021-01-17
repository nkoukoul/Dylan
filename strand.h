#ifndef STRAND_H
#define STRAND_H

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

struct strand;

struct job
{
    struct job *next_;
    void (*func_ptr_)(int, struct strand *);
    int job_id_;
};

struct strand
{
    struct job *head_;
    pthread_mutex_t strand_lock_;
    int size_;
    int job_id_;
};

pthread_mutex_t initialize_lock(struct strand * sd)
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

struct strand *create_strand()
{
    struct strand *sd;
    sd = (struct strand *)malloc(sizeof(struct strand));
    struct job *head; /*empty event*/
    head = (struct job *)malloc(sizeof(struct job));
    head->next_ = NULL;
    head->job_id_ = -1;
    sd->head_ = head;
    sd->size_ = 0;
    sd->job_id_ = 0;
    sd->strand_lock_ = initialize_lock(sd);
    return sd;
}

void add_job(struct strand *sd, void (*func) (int, struct strand *))
{
    struct job *new_job;
    new_job = (struct job *)malloc(sizeof(struct job));
    new_job->next_ = sd->head_->next_;
    new_job->func_ptr_ = func;
    pthread_mutex_lock(&sd->strand_lock_);
    sd->head_->next_ = new_job;
    sd->size_++;
    sd->job_id_++;
    new_job->job_id_ = sd->job_id_;
    pthread_mutex_unlock(&sd->strand_lock_);
}

struct job *get_job(struct strand *sd)
{
    pthread_mutex_lock(&sd->strand_lock_);
    if (!sd->head_->next_)
    {
        pthread_mutex_unlock(&sd->strand_lock_);
        return NULL;
    }
    struct job *first_job = sd->head_->next_;
    sd->head_->next_ = first_job->next_;
    sd->size_--;
    pthread_mutex_unlock(&sd->strand_lock_);
    return first_job;
}

#endif //STRAND_H