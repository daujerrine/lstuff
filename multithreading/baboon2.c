/*
 * The following text has been copied verbatim from the little book of
 * semaphores. This text is present here for reference purposes only.
 * -----------------------------------------------------------------------------
 *
 * There is a deep canyon somewhere in Kruger National Park, South Africa, and a
 * single rope that spans the canyon. Baboons can cross the canyon by swinging
 * hand-over-hand on the rope, but if two baboons going in opposite directions
 * meet in the middle, they will fight and drop to their deaths. Furthermore,
 * the rope is only strong enough to hold 5 baboons. If there are more baboons
 * on the rope at the same time, it will break. Assuming that we can teach the
 * baboons to use semaphores, we would like to design a synchronization scheme
 * with the following properties:
 * * Once a baboon has begun to cross, it is guaranteed to get to the other
 *   side without running into a baboon going the other way.
 * * There are never more than 5 baboons on the rope.
 * * A continuing stream of baboons crossing in one direction should not bar
 *   baboons going the other way indefinitely (no starvation).
 *
 * We assume there are two ends of the rope, left and right, where the baboons
 * are sitting.
 *
 * Implementation is most likely wrong.
 */

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>

#define MAX_BUF 4096

#define MSGM(m, ...) printf("MAIN: " m, ## __VA_ARGS__)
#define MSGB(n, m, ...) printf("BABOON %d: " m, n, ## __VA_ARGS__)

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

int next = 0;
int running_threads = 0;


typedef enum Direction {
    DIRECTION_INACTIVE,
    DIRECTION_LEFT,
    DIRECTION_RIGHT
} Direction;

typedef struct BaboonRecord {
    int num;
    int direction;
} BaboonRecord;

Queue left_rope_lock, right_rope_lock;
int left_baboon_count = 0, right_baboon_count = 0;
int next_direction = DIRECTION_INACTIVE; // We start with the baboons on the left
pthread_mutex_t update_lock;
sem_t cross_lock;

/******************************************************************************/

/******************************* FUNCTIONS ************************************/

void cross_rope()
{
    usleep(10000 * (rand() % 10));
    printf("**** CROSS_ROPE called\n");
}

void *baboon(void *r)
{
    BaboonRecord *b = (BaboonRecord *) r;
    int num = b->num;
    int direction = b->direction;
    next = 0;

    int initiator = 0;

    sem_t self_lock;
    sem_t *lock;
    sem_init(&self_lock, 0, 0);

    MSGB(num, "I am Baboon %d, going %s\n", num, direction == DIRECTION_LEFT ? "right" : "left");

    pthread_mutex_lock(&update_lock);
    if (direction == DIRECTION_LEFT) {
        left_baboon_count++;
        q_enqueue(&left_rope_lock, &self_lock);
        if (left_baboon_count == 5) {
            MSGB(num, "5 baboons on the left side. Seeing if we can cross.\n");
            sem_wait(&cross_lock);
            MSGB(num, "Looks like we can proceed now. Signalling Everyone.\n");
            for (int i = 0; i < 5; i++) {
                q_dequeue(&left_rope_lock, &lock);
                sem_post(lock);
            }
            left_baboon_count = 0;
            initiator = 1;
        }
    } else if (direction == DIRECTION_RIGHT) {
        right_baboon_count++;
        q_enqueue(&right_rope_lock, &self_lock);
        if (right_baboon_count == 5) {
            MSGB(num, "5 baboons on the right side. Seeing if we can cross.\n");
            sem_wait(&cross_lock);
            MSGB(num, "Looks like we can proceed now. Signalling Everyone.\n");
            for (int i = 0; i < 5; i++) {
                q_dequeue(&right_rope_lock, &lock);
                sem_post(lock);
            }
            right_baboon_count = 0;
            initiator = 1;
        }
    }
    pthread_mutex_unlock(&update_lock);

    sem_wait(&self_lock);

    cross_rope();

    if (direction == DIRECTION_LEFT)
        MSGB(num, "Reached the right side.\n");
    else if (direction == DIRECTION_RIGHT)
        MSGB(num, "Reached the left side.\n");

    if (initiator) {
        MSGB(num, "I initiated the crossing. Returning the lock on the rope.\n");
        sem_post(&cross_lock);
    }

    MSGB(num, "Everything done. Exiting\n");

    sem_destroy(&self_lock);
    running_threads--;
}

/******************************************************************************/

int main()
{
    int ret, nleftbaboon, nrightbaboon;
    int iter = 0;
    srand(time(NULL));
    BaboonRecord b;
    pthread_t leftbaboon_threads[10], rightbaboon_threads[10];
    pthread_mutex_init(&update_lock, NULL);
    q_init(&left_rope_lock, 10);
    q_init(&right_rope_lock, 10);
    sem_init(&cross_lock, 0, 1);
    printf("Enter number of baboons on left: ");
    scanf("%d", &nleftbaboon);
    printf("Enter number of baboons on right: ");
    scanf("%d", &nrightbaboon);

    MSGM("Starting Threads\n");

    for (int i = 0; i < nleftbaboon; i++) {
        b.direction = DIRECTION_LEFT;
        b.num = iter; iter++;
        ret = pthread_create(&leftbaboon_threads[i], NULL, baboon, (void *) &b);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating left baboon\n");
            return -1;
        }
        running_threads++;
    }

    for (int i = 0; i < nrightbaboon; i++) {
        b.direction = DIRECTION_RIGHT;
        b.num = iter; iter++;
        ret = pthread_create(&rightbaboon_threads[i], NULL, baboon, (void *) &b);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating right baboon\n");
            return -1;
        }
        running_threads++;
    }


    while (running_threads > 0);
    MSGM("All threads exited. Exiting\n");
    pthread_exit(NULL);
    return 0;
}
