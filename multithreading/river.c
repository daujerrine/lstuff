/*
 * The following text has been copied verbatim from the little book of
 * semaphores. This text is present here for reference purposes only.
 * -----------------------------------------------------------------------------
 *
 * Somewhere near Redmond, Washington there is a rowboat that is used by
 * both Linux hackers and Microsoft employees (serfs) to cross a river. The ferry
 * can hold exactly four people; it won’t leave the shore with more or fewer. To
 * guarantee the safety of the passengers, it is not permissible to put one hacker
 * in the boat with three serfs, or to put one serf with three hackers. Any other
 * combination is safe.
 *
 * As each thread boards the boat it should invoke a function called board. You
 * must guarantee that all four threads from each boatload invoke board before
 * any of the threads from the next boatload do.
 *
 * After all four threads have invoked board, exactly one of them should call
 * a function named rowBoat, indicating that that thread will take the oars. It
 * doesn’t matter which thread calls the function, as long as one does.
 * Don’t worry about the direction of travel. Assume we are only interested in
 * traffic going in one of the directions.
 */

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_BUF 4096

#define MSGM(m, ...) printf("MAIN: " m, ## __VA_ARGS__)
#define MSGH(n, m, ...) printf("HACKER %d: " m, n, ## __VA_ARGS__)
#define MSGS(n, m, ...) printf("SERF %d: " m, n, ## __VA_ARGS__)

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

Queue h_lock, s_lock;
int hacker_count = 0;
int serf_count = 0;
int running_threads = 0;
int next = 0;

pthread_mutex_t boat_mutex;

/******************************************************************************/


/******************************* FUNCTIONS ************************************/

void board() {
    printf("**** BOARD Called.\n");
}

void row_boat() {
    printf("**** ROW_BOAT Called.\n");
}


void *hacker(void *n)
{
    int num = *((int *) n);
    next = 0;

    int is_captain = 0;

    sem_t self_lock;
    sem_t *lock;

    MSGH(num, "I am Hacker %d\n", num);

    MSGH(num, "At dock\n");
    sem_init(&self_lock, 0, 0);

    pthread_mutex_lock(&boat_mutex);
    hacker_count += 1;
    q_enqueue(&h_lock, &self_lock);
    if (hacker_count == 4) {
        MSGH(num, "Requirements satisfied. Starting Boat\n");
        q_dequeue(&h_lock, &lock);
        sem_post(lock);
        q_dequeue(&h_lock, &lock);
        sem_post(lock);
        q_dequeue(&h_lock, &lock);
        sem_post(lock);
        q_dequeue(&h_lock, &lock);
        sem_post(lock);

        hacker_count = 0;
        is_captain = 1;
    } else if (hacker_count == 2 && serf_count >= 2) {
        MSGH(num, "Requirements satisfied with serfs. Starting Boat\n");
        q_dequeue(&h_lock, &lock);
        sem_post(lock);
        q_dequeue(&h_lock, &lock);
        sem_post(lock);
        q_dequeue(&s_lock, &lock);
        sem_post(lock);
        q_dequeue(&s_lock, &lock);
        sem_post(lock);

        serf_count -= 2;
        hacker_count = 0;
        is_captain = 1;
    } else {
        MSGH(num, "Giving up lock\n");
        pthread_mutex_unlock(&boat_mutex);
    }

    sem_wait(&self_lock);

    MSGH(num, "Boarding\n");
    board();

    if (is_captain) {
        MSGH(num, "I am captain. Rowing boat.\n");
        row_boat();
        pthread_mutex_unlock(&boat_mutex);
    }

    MSGH(num, "Done. Exiting\n");
    sem_destroy(&self_lock);
    running_threads--;
}

void *serf(void *n)
{
    int num = *((int *) n);
    next = 0;

    int is_captain = 0;

    sem_t self_lock;
    sem_t *lock;

    MSGS(num, "I am Serf %d\n", num);

    MSGS(num, "At dock\n");
    sem_init(&self_lock, 0, 0);

    pthread_mutex_lock(&boat_mutex);
    serf_count += 1;
    q_enqueue(&s_lock, &self_lock);
    if (serf_count == 4) {
        MSGS(num, "Requirements satisfied. Starting Boat\n");
        q_dequeue(&s_lock, &lock);
        sem_post(lock);
        q_dequeue(&s_lock, &lock);
        sem_post(lock);
        q_dequeue(&s_lock, &lock);
        sem_post(lock);
        q_dequeue(&s_lock, &lock);
        sem_post(lock);

        serf_count = 0;
        is_captain = 1;
    } else if (serf_count == 2 && hacker_count >= 2) {
        MSGS(num, "Requirements satisfied with hackers. Starting Boat\n");
        q_dequeue(&s_lock, &lock);
        sem_post(lock);
        q_dequeue(&s_lock, &lock);
        sem_post(lock);
        q_dequeue(&s_lock, &lock);
        sem_post(lock);
        q_dequeue(&s_lock, &lock);
        sem_post(lock);

        hacker_count -= 2;
        serf_count = 0;
        is_captain = 1;
    } else {
        MSGS(num, "Giving up lock\n");
        pthread_mutex_unlock(&boat_mutex);
    }

    sem_wait(&self_lock);

    MSGS(num, "Boarding\n");
    board();

    if (is_captain) {
        MSGS(num, "I am captain. Rowing boat.\n");
        row_boat();
        pthread_mutex_unlock(&boat_mutex);
    }

    MSGS(num, "Done. Exiting\n");
    sem_destroy(&self_lock);
    running_threads--;
}

/******************************************************************************/

int main()
{
    int ret, nhacker, nserf;
    pthread_t serf_threads[10], hacker_threads[10];
    pthread_mutex_init(&boat_mutex, NULL);
    q_init(&s_lock, 10);
    q_init(&h_lock, 10);
    printf("Enter number of serfs and hackers (in equal amount).\n" \
           "Make sure this number, times two is a multiple of four: ");
    scanf("%d", &nserf);
    nhacker = nserf;

    MSGM("Starting Threads\n");
    for (int i = 0; i < nserf; i++) {
        ret = pthread_create(&serf_threads[i], NULL, serf, (void *) &i);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating serf\n");
            return -1;
        }
        running_threads++;
    }

    for (int i = 0; i < nhacker; i++) {
        ret = pthread_create(&hacker_threads[i], NULL, hacker, (void *) &i);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating hacker\n");
            return -1;
        }
        running_threads++;
    }


    while (running_threads > 0);
    MSGM("All threads exited. Exiting\n");
    pthread_exit(NULL);
    return 0;
}

