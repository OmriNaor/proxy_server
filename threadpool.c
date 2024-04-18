#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "threadpool.h"



/**
 * Dequeues a work item from the thread pool's queue.
 *
 * @param tp: A pointer to the threadpool structure from which to dequeue a work item.
 * @return
 *   - A pointer to the dequeued work_t structure if the queue is not empty.
 *   - NULL if the queue is empty or if the threadpool pointer is NULL.
 */
work_t* dequeue(threadpool* tp)
{
    if (tp == NULL)
        return NULL;

    // Check if the queue is empty.
    if (tp->qhead == NULL)
    {
        int lock_result = pthread_mutex_unlock(&tp->qlock); // Unlock the queue before returning.
        if (lock_result != 0)
        {
            fprintf(stderr, "Error unlocking mutex: %d\n", lock_result);
            pthread_exit(NULL);
        }

        return NULL; // Return NULL if the queue is empty.
    }

    // Get the first work_t item from the queue.
    work_t* work = tp->qhead;
    tp->qhead = work->next; // Update the head of the queue to the next element.

    if (tp->qhead == NULL)
        tp->qtail = NULL; // If the queue is now empty, also update the tail to NULL.


    tp->qsize--; // Decrement the size of the queue.

    work->next = NULL; // Clear the next pointer of the dequeued work_t item.

    return work; // Return the dequeued work_t item.
}

/**
 * Enqueues a work item to the thread pool's queue.
 *
 * @param tp: A pointer to the threadpool structure to which a work item is enqueued.
 * @param work: A pointer to the work_t structure to be enqueued.
 */
void enqueue(threadpool* tp, work_t* work)
{
    if (tp == NULL || work == NULL)
        return;


    work->next = NULL; // Ensure the next pointer is null since it's going to be the new tail.

    if (tp->qtail == NULL)
        // The queue is empty, so this work item will be both the head and tail.
        tp->qhead = tp->qtail = work;
    else
    {
        // The queue is not empty, append the work item to the end and update the tail.
        tp->qtail->next = work;
        tp->qtail = work;
    }

    tp->qsize++; // Increment the queue size.
}

/**
 * Dispatches a new work item to the thread pool.
 *
 * @param from_me: A pointer to the threadpool structure to which the work item is dispatched.
 * @param dispatch_to_here: A function pointer to the routine that should be executed by the work item.
 * @param arg: A pointer to the argument that should be passed to the routine.
 */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
    int lock_result = pthread_mutex_lock(&from_me->qlock);
    if (lock_result != 0)
        fprintf(stderr, "Error locking mutex: %d\n", lock_result);

    if (from_me->dont_accept)
        return;



    work_t* work = malloc(sizeof(work_t));
    if (work == NULL)
    {
        printf("failed");
        return;
    }

    work->routine = dispatch_to_here;
    work->arg = arg;

    enqueue(from_me, work);
    lock_result = pthread_mutex_unlock(&from_me->qlock);
    if (lock_result != 0)
        fprintf(stderr, "Error unlocking mutex: %d\n", lock_result);

    int cond_result = pthread_cond_signal(&from_me->q_not_empty);
    if (cond_result != 0)
        fprintf(stderr, "Error signaling condition variable: %d\n", cond_result);
}

/**
 * The worker function for each thread in the pool, executing dispatched work items.
 *
 * @param p: A void pointer to the threadpool structure.
 * @return
 *   - NULL always, as it serves as the pthreads routine which does not return a value.
 */
void* do_work(void* p)
{
    threadpool* tp = (threadpool*) p;

    while(1)
    {
        int lock_result = pthread_mutex_lock(&tp->qlock);
        if (lock_result != 0)
        {
            fprintf(stderr, "Error locking mutex: %d\n", lock_result);
            pthread_exit(NULL);
        }


        // Wait for work to be available or for shutdown signal.
        while (tp->qsize == 0 && !tp->shutdown)
        {
            int cond_result = pthread_cond_wait(&tp->q_not_empty, &tp->qlock);
            if (cond_result != 0)
            {
                fprintf(stderr, "Error waiting condition variable: %d\n", cond_result);
                pthread_exit(NULL);
            }
        }


        if (tp->shutdown)
        {
            lock_result = pthread_mutex_unlock(&tp->qlock);
            if (lock_result != 0)
                fprintf(stderr, "Error unlocking mutex: %d\n", lock_result);

            pthread_exit(NULL);
        }

        work_t* work = dequeue(tp);

        // Signal if the pool is being destroyed and no more tasks are pending.
        if (tp->dont_accept && tp->qsize == 0)
        {
            int cond_result = pthread_cond_signal(&tp->q_empty);
            if (cond_result != 0)
            {
                fprintf(stderr, "Error signaling condition variable: %d\n", cond_result);
                pthread_exit(NULL);
            }
        }

        lock_result = pthread_mutex_unlock(&tp->qlock);
        if (lock_result != 0)
        {
            fprintf(stderr, "Error unlocking mutex: %d\n", lock_result);
            pthread_exit(NULL);
        }


        if(work != NULL)
        {
            work->routine(work->arg);
            free(work);
        }
    }
}


/**
 * Gracefully shuts down the thread pool, waiting for all queued work items to be completed before freeing resources.
 *
 * @param destroyme: A pointer to the threadpool structure to be destroyed.
 */
void destroy_threadpool(threadpool* destroyme)
{
    int lock_result = pthread_mutex_lock(&destroyme->qlock);
    if (lock_result != 0)
        fprintf(stderr, "Error locking mutex: %d\n", lock_result);

    destroyme->dont_accept = 1;

    if(destroyme->qsize > 0)
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);

    destroyme->shutdown = 1;
    pthread_mutex_unlock(&destroyme->qlock);
    pthread_cond_broadcast(&destroyme->q_not_empty);

    for (int i = 0 ; i < destroyme->num_threads ; i++)
        pthread_join(destroyme->threads[i], NULL);

    free (destroyme->threads);
    pthread_mutex_destroy(&destroyme->qlock);
    pthread_cond_destroy(&destroyme->q_not_empty);
    pthread_cond_destroy(&destroyme->q_empty);
    free(destroyme);
}


/**
 * Creates and initializes a thread pool with a specified number of worker threads.
 *
 * @param num_threads_in_pool: The number of worker threads to be created in the thread pool.
 * @return
 *   - A pointer to the created threadpool structure.
 *   - NULL if the initialization fails due to invalid input or memory allocation failure.
 */
threadpool* create_threadpool(int num_threads_in_pool)
{
    if (num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool < 1)
    {
        printf("Invalid amount of threads. Maximum is %d\n", MAXT_IN_POOL);
        return NULL;
    }

    threadpool* tp = malloc(sizeof(threadpool));
    if (tp == NULL)
    {
        printf("Failed to allocate threadpool\n");
        exit(EXIT_FAILURE);
    }

    tp->num_threads = num_threads_in_pool;
    tp->qsize = 0;
    tp->threads = (pthread_t*) malloc(num_threads_in_pool * sizeof(pthread_t));
    if (tp->threads == NULL)
    {
        free(tp);
        printf("Failed to allocate threads array\n");
        exit(EXIT_FAILURE);
    }

    tp->qhead = NULL;
    tp->qtail = NULL;
    if (pthread_mutex_init(&tp->qlock, NULL) != 0 ||
        pthread_cond_init(&tp->q_empty, NULL) != 0 ||
        pthread_cond_init(&tp->q_not_empty, NULL) != 0)
    {
        // Clean up in case of failure
        free(tp->threads);
        free(tp);
        printf("Failed to initialize mutex or condition variable\n");
        exit(EXIT_FAILURE);
    }
    tp->shutdown = 0;
    tp->dont_accept = 0;

    for (int i = 0 ; i < num_threads_in_pool ; i++)
        if (pthread_create(&tp->threads[i], NULL, do_work, (void*)tp) != 0)
        {
            // On failure, cancel and join any threads already created, then clean up
            for (int j = 0; j < i; j++)
            {
                pthread_cancel(tp->threads[j]);
                pthread_join(tp->threads[j], NULL);
            }

            free(tp->threads);
            free(tp);
            printf("Failed to create all threads\n");
            exit(EXIT_FAILURE);
        }

    return tp;
}


