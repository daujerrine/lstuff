/*
 * The following text has been copied verbatim from the little book of
 * semaphores. This text is present here for reference purposes only.
 * -----------------------------------------------------------------------------
 *
 * There are two kinds of threads, oxygen and hydrogen. In order to assemble
 * these threads into water molecules, we have to create a barrier that makes
 * each thread wait until a complete molecule is ready to proceed.
 *
 * As each thread passes the barrier, it should invoke bond. You must guarantee
 * that all the threads from one molecule invoke bond before any of the threads
 * from the next molecule do.
 *
 * In other words:
 * * If an oxygen thread arrives at the barrier when no hydrogen threads are
 *   present, it has to wait for two hydrogen threads.
 * * If a hydrogen thread arrives at the barrier when no other threads are
 *   present, it has to wait for an oxygen thread and another hydrogen thread.
 * We don’t have to worry about matching the threads up explicitly; that is,
 * the threads do not necessarily know which other threads they are paired up
 * with. The key is just that threads pass the barrier in complete sets; thus,
 * if we examine the sequence of threads that invoke bond and divide them into
 * groups of three, each group should contain one oxygen and two hydrogen
 * threads.
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_BUF 4096

#define MSGM(m, ...) printf("MAIN: " m, ## __VA_ARGS__)
#define MSGH(n, m, ...) printf("HYDROGEN %d: " m, n, ## __VA_ARGS__)
#define MSGO(n, m, ...) printf("OXYGEN %d: " m, n, ## __VA_ARGS__)

/*************************** QUEUE IMPLEMENTATION *****************************/

typedef struct Queue {
    sem_t *data[MAX_BUF];
    int front;
    int rear;
    int size;
} Queue;

void q_init(Queue *q, int size)
{
    q->front = -1;
    q->rear = -1;
    q->size = size;
}

int q_enqueue(Queue *q, sem_t *val)
{
    if(((q->rear + 1) % q->size) == (q->front)) {
        printf("(!!!) QUEUE FULL\n");
        return -1;
    }

    if(q->rear == -1) {
        q->front = ++(q->rear);
        q->data[q->rear] = val;
    } else {
        q->rear = (q->rear + 1) % q->size;
        q->data[q->rear] = val;
    }
}

int q_dequeue(Queue *q, sem_t **dest)
{
    int rval;

    if(q->front == -1) {
        printf("(!!!) QUEUE EMPTY\n");
        return -1;
    } else if(q->front == q->rear) {
        *dest = q->data[q->front];
        q->front = -1;
        q->rear  = -1;
    } else {
        *dest = q->data[q->front];
        q->front = (q->front + 1) % q->size;
    }

    return 1;
}

int q_full(Queue *q)
{
    return (((q->rear + 1) % q->size) == (q->front));
}

int q_empty(Queue *q)
{
    return q->rear == -1;
}

/******************************************************************************/


/***************************** SHARED VARIABLES *******************************/

Queue h_lock, o_lock;
int oxygen_count = 0;
int hydrogen_count = 0;
int running_threads = 0;
int next = 0;

pthread_mutex_t bond_mutex;

/******************************************************************************/


/******************************* FUNCTIONS ************************************/

void bond() {
    printf("**** BOND Called.\n");
}

void *hydrogen(void *n)
{
    int num = *((int *) n);
    next = 0;

    sem_t self_lock;
    sem_t *lock;

    MSGH(num, "I am Hydrogen %d\n", num);

    MSGH(num, "Registering at barrier\n");
    sem_init(&self_lock, 0, 0);

    pthread_mutex_lock(&bond_mutex);
    hydrogen_count += 1;
    q_enqueue(&h_lock, &self_lock);
    if (hydrogen_count >= 2 && oxygen_count >= 1) {
        MSGH(num, "Requirements satisfied. Creating Bond.\n");
        q_dequeue(&h_lock, &lock);
        sem_post(lock);
        q_dequeue(&h_lock, &lock);
        sem_post(lock);
        hydrogen_count -= 2;

        q_dequeue(&o_lock, &lock);
        sem_post(lock);
        oxygen_count -= 1;
    }
    pthread_mutex_unlock(&bond_mutex);

    sem_wait(&self_lock);
    bond();
    MSGH(num, "H2O Made. Exiting\n");

    sem_destroy(&self_lock);
    running_threads--;
}

void *oxygen(void *n)
{
    int num = *((int *) n);
    next = 0;

    sem_t self_lock;
    sem_t *lock;

    MSGO(num, "I am Oxygen %d\n", num);

    MSGO(num, "Registering at barrier\n");
    sem_init(&self_lock, 0, 0);

    pthread_mutex_lock(&bond_mutex);
    oxygen_count += 1;
    q_enqueue(&o_lock, &self_lock);
    if (hydrogen_count >= 2) {
        MSGO(num, "Requirements satisfied. Creating Bond.\n");
        q_dequeue(&o_lock, &lock);
        sem_post(lock);
        q_dequeue(&o_lock, &lock);
        sem_post(lock);
        hydrogen_count -= 2;

        q_dequeue(&o_lock, &lock);
        sem_post(lock);
        oxygen_count -= 1;
    }
    pthread_mutex_unlock(&bond_mutex);

    sem_wait(&self_lock);
    bond();
    MSGO(num, "H2O Made. Exiting\n");

    sem_destroy(&self_lock);
    running_threads--;
}

/******************************************************************************/

int main()
{
    int ret, nhydrogen, noxygen;
    pthread_t oxygen_threads[10], hydrogen_threads[20];
    pthread_mutex_init(&bond_mutex, NULL);
    q_init(&o_lock, 10);
    q_init(&h_lock, 20);
    printf("Enter number of resultant molecules: ");
    scanf("%d", &noxygen);

    nhydrogen = noxygen * 2;

    MSGM("Starting Threads\n");
    for (int i = 0; i < noxygen; i++) {
        ret = pthread_create(&oxygen_threads[i], NULL, oxygen, (void *) &i);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating oxygen\n");
            return -1;
        }
        running_threads++;
    }

    for (int i = 0; i < nhydrogen; i++) {
        ret = pthread_create(&hydrogen_threads[i], NULL, hydrogen, (void *) &i);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating hydrogen\n");
            return -1;
        }
        running_threads++;
    }


    while (running_threads > 0);
    MSGM("All threads exited. Exiting\n");
    pthread_exit(NULL);
    return 0;
}
