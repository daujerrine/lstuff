/*
 * Our barbershop has:
 *  + three chairs
 *  + three barbers, and
 *  + a waiting area that can accommodate four customers on a sofa, and
 *    + that has standing room for additional customers.
 *
 * - Fire codes limit the total number of customers in the shop to 20.
 *
 * - A customer will not enter the shop if it is filled to capacity.
 *
 * - Once inside, the customer takes a seat on the sofa or stands if the
 *   sofa is filled.
 *
 * - When a barber is free, the customer that has been on the sofa the
 *   longest is served and, if there are any standing customers, the one
 *   that has been in the shop the longest takes a seat on the sofa.
 *
 * - When a customer’s haircut is finished, any barber can accept payment,
 *   but because there is only one cash register, payment is accepted for
 *   one customer at a time.
 *
 * - The barbers divide their time among cutting hair, accepting payment,
 *   and sleeping in their chair waiting for a customer.
 *
 * 1. Customers invoke the following functions in order: enterShop, sitOnSofa,
 *    getHairCut, pay.
 * 2. Barbers invoke cutHair and acceptPayment.
 * 3. Customers cannot invoke enterShop if the shop is at capacity.
 * 4. If the sofa is full, an arriving customer cannot invoke sitOnSofa.
 * 5. When a customer invokes getHairCut there should be a corresponding barber
 *    executing cutHair concurrently, and vice versa.
 * 6. It should be possible for up to three customers to execute getHairCut
 *    concurrently, and up to three barbers to execute cutHair concurrently.
 * 7. The customer has to pay before the barber can acceptPayment.
 * 8. The barber must acceptPayment before the customer can exit.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_BUF 4096

#define MSGM(m, ...) printf("MAIN: " m, ## __VA_ARGS__)
#define MSGB(n, m, ...) printf("BARBER %d: " m, n, ## __VA_ARGS__)
#define MSGC(n, m, ...) printf("CUSTOMER %d: " m, (n)->num, ## __VA_ARGS__)


/************************* SEMAPHORE IMPLEMENTATION ***************************/

#define DEBUG 0

#if DEBUG == 1
#   define LOGTXT(_m, _s, _v) printf(_m " %s v= %d\n", (_s), (_v));
#else
#   define LOGTXT(_m, _s, _v)
#endif

typedef struct Semaphore {
    int state;
    char tag[20];
} Semaphore;

void sem_wait(Semaphore *s)
{
    while(s->state <= 0);
    s->state--;
    LOGTXT("waited", s->tag, s->state);
}

void sem_signal(Semaphore *s)
{
    LOGTXT("signalled", s->tag, s->state);
    s->state++;
}

// Static/Compile time Initializer
#define DEF_SEMAPHORE(_tag, _value) Semaphore _tag = { .tag = #_tag, .state = _value }


// Runtime Initializer
void sem_init(Semaphore *s, int value) {
    s->state = value;
    s->tag[0] = 'q';
    s->tag[1] = '\0';
}

/******************************************************************************/


/*************************** QUEUE IMPLEMENTATION *****************************/

typedef struct Queue {
    Semaphore *data[MAX_BUF];
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

int q_enqueue(Queue *q, Semaphore *val)
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

int q_dequeue(Queue *q, Semaphore **dest)
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

/**************************** SHARED VARIABLES ********************************/

int next = 0;

int running_threads = 0;
const int total_capacity = 20;
const int total_barbers = 3;
const int total_sofas = 4;
const int total_standing = total_capacity - total_sofas;

int haircut_delay = 100;
int pay_delay = 100;

int customer_left = 1;
int customer_count = 0;


Queue sofa_queue;
Queue standing_queue;


DEF_SEMAPHORE(sofa, 4);
DEF_SEMAPHORE(payment, 0);
DEF_SEMAPHORE(receipt, 0);
DEF_SEMAPHORE(standing_customer, 0);
DEF_SEMAPHORE(sofa_customer, 0);

pthread_mutex_t queue_mutex, sofa_queue_mutex, transaction_mutex;

/******************************************************************************/


/*************************** BARBER FUNCTIONS *********************************/

void b_cut_hair(int barb_num)
{
    MSGB(barb_num, "Cutting Hair\n");
    usleep(haircut_delay);
    MSGB(barb_num, "Done Cutting Hair. Telling customer to go to register.\n");
    sem_wait(&payment);
}

// acceptPayment
void b_accept_payment(int barb_num)
{
    sem_signal(&receipt);
    MSGB(barb_num, "Received Payment.\n");
}

void *barber(void *n)
{
    int barb_num = *((int *) n);
    next = 0;
    Semaphore *s;
    MSGB(barb_num, "I am barber %d\n", barb_num);
    int loop = 1;
    while (1) {

        // Wait for any customers
        // We probe bith the semaphore and the customer left here.

        while (standing_customer.state <= 0) {
            if (customer_left <= 0) {
                loop = 0;
                break;
            }
        }

        // If no customers left, exit
        if (!loop)
            break;
        else
            standing_customer.state--;

        while (1) {
            // Dequeue Customer, signal customer to sit on sofa and wait until
            // customer sits on sofa.
            pthread_mutex_lock(&queue_mutex);
            if (q_empty(&standing_queue)) {
                pthread_mutex_unlock(&queue_mutex);
                continue;
            }
            q_dequeue(&standing_queue, &s);
            sem_signal(s);
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        // Wait for any customers on sofa
        sem_wait(&sofa_customer);

        while (1) {
            //printf("* %d\n", __LINE__);
            // Dequeue Customer, signal customer to sit on sofa and wait until
            // customer sits on sofa.
            pthread_mutex_lock(&sofa_queue_mutex);
            //printf("* %d\n", __LINE__);
            if (q_empty(&sofa_queue)) {
                pthread_mutex_unlock(&sofa_queue_mutex);
                continue;
            }
            q_dequeue(&sofa_queue, &s);
            sem_signal(s);
            pthread_mutex_unlock(&sofa_queue_mutex);
            break;
        }

        // Cut hair
        b_cut_hair(barb_num);
        // Make Payment
        b_accept_payment(barb_num);
    }

    MSGB(barb_num, "All Customers Served. Exiting.\n");
    running_threads--;
}

/******************************************************************************/

/************************** CUSTOMER FUNCTIONS ********************************/

typedef struct Customer {
    int num;
    Semaphore stand_done;  // Wait till we are out of standing queue
    Semaphore sofa_done;   // Wait till we are out of sofa queue
} Customer;

// enterShop
int c_enter_shop(Customer *s)
{
    pthread_mutex_lock(&queue_mutex);
    if (customer_count >= total_capacity) {
        MSGC(s, "Store Full. Exiting\n");
        pthread_mutex_unlock(&queue_mutex);
        return -1;
    }
    MSGC(s, "Entering Shop\n");
    customer_count++;
    q_enqueue(&standing_queue, &s->stand_done);
    pthread_mutex_unlock(&queue_mutex);

    // MSGC(s, "Queueing Done\n");
    sem_signal(&standing_customer);
    // printf(">> %d 0x%lx\n", __LINE__,  (long unsigned int) &s->stand_done);
    sem_wait(&s->stand_done);
    MSGC(s, "Going to Sofa\n");
}

// sitOnSofa
void c_sit_on_sofa(Customer *s)
{
    sem_wait(&sofa); // Wait till lock on sofa is released

    pthread_mutex_lock(&sofa_queue_mutex);
    q_enqueue(&sofa_queue, &s->sofa_done);
    pthread_mutex_unlock(&sofa_queue_mutex);
    MSGC(s, "Sitting on Sofa\n");

    sem_signal(&sofa_customer);
    sem_wait(&s->sofa_done);
    customer_count--;
    sem_signal(&sofa);
    MSGC(s, "Going to Barber Chair\n");
}

// getHairCut
void c_get_haircut(Customer *s)
{
    pthread_mutex_lock(&queue_mutex);
    sem_signal(&payment);
}

// pay
void c_pay(Customer *s)
{
    sem_wait(&receipt);
    MSGC(s, "Got Receipt\n");
    customer_left--;
    pthread_mutex_unlock(&queue_mutex);
}

void *customer(void *n)
{
    int cust_num = *((int *) n);
    next = 0;
    Customer s;
    s.num = cust_num;
    sem_init(&s.stand_done, 0);
    sem_init(&s.sofa_done, 0);
    MSGC(&s, "I am Customer %d\n", s.num);
    if (c_enter_shop(&s) < 0) {
        pthread_mutex_lock(&queue_mutex);
        customer_left--;
        pthread_mutex_unlock(&queue_mutex);
        running_threads--;
        return NULL;
    }
    c_sit_on_sofa(&s);
    c_get_haircut(&s);
    c_pay(&s);

    MSGC(&s, "Exiting\n");
    running_threads--;
}

/******************************************************************************/

int main(int argc, char **argv)
{
    int ret, num_customers;
    pthread_t barber_threads[3], customer_threads[30];

    printf("There are:\n===========\n" \
           "%d Barbers\n" \
           "%d Sofas, and,\n" \
           "A shop with a max capacity of %d, and hence %d standing places.\n\n", \
           total_barbers, total_sofas, total_capacity, total_standing);
    printf("Enter delay in milliseconds for haircut: ");
    scanf("%d", &haircut_delay);
    haircut_delay *= 1000;
    printf("Enter delay in milliseconds for payment: ");
    scanf("%d", &pay_delay);
    pay_delay *= 1000;
    printf("Enter number of customers: ");
    scanf("%d", &num_customers);
    customer_left = num_customers;

    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&sofa_queue_mutex, NULL);
    q_init(&sofa_queue, total_sofas);
    q_init(&standing_queue, total_capacity - total_sofas);

    MSGM("Starting Threads\n");
    for (int i = 0; i < total_barbers; i++) {
        ret = pthread_create(&barber_threads[i], NULL, barber, (void *) &i);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating barber\n");
            return -1;
        }
        running_threads++;
    }

    for (int i = 0; i < num_customers; i++) {
        ret = pthread_create(&customer_threads[i], NULL, customer, (void *) &i);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating customer\n");
            return -1;
        }
        running_threads++;
    }

    while (running_threads > 0);
    MSGM("All threads exited. Exiting\n");
    pthread_exit(NULL);
    return 0;
}
