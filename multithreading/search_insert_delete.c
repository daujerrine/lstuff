/*
 * Search/Insert/Delete Problem: Three kinds of processes share access to a
 * single-linked list: searchers, inserter, and deleters. Searchers merely
 * examine the list; hence they can execute concurrently with each other.
 * Inserters add new items to the end of the list; insertions must be mutually
 * exclusive to prelude two inserters from inserting new items at about the same
 * time. However, one insert can proceed in parallel with any number of
 * searches. Finally, deleters remove items from anywhere in the list. At most
 * one deleter process can access the list at a time, and deletion must also be
 * mutually exclusive with searches and insertions.
 *
 * ============================================================================
 *
 * Shared Variables
 * ================
 * LinkedList list; -> the shared linked list structure
 * int next = 0; ->
 * int iter = 0; -> Next number to put in the list. Usually iterated
 * #define MODE_SEARCH 2 -> Search mode constant. This means current active threads reading is a search function(s)
 * #define MODE_INSERT 1 -> Current active thread writing to the list is an insert function
 * #define MODE_DELETE 0 -> active thread is a delete function
 * int current_mode = -1; -> Variable to store current mode
 * int active = 0; -> is the current mode active?
 * pthread_mutex_t write_mutex, mode_mutex; -> Mutexes on the writing lock on the list, and the mode variables
 * int active_searchers; -> count of active searchers
 * int running_threads = 0; -> counter having running threads
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#define MSGM(m, ...) printf("MAIN: " m, ## __VA_ARGS__)
#define MSGI(n, m, ...) printf("INSERTER %d: " m, n, ## __VA_ARGS__)
#define MSGS(n, m, ...) printf("SEARCHER %d: " m, n, ## __VA_ARGS__)
#define MSGD(n, m, ...) printf("DELETER %d: " m, n, ## __VA_ARGS__)


/**************************** LLIST IMPLEMENTATION ****************************/

struct Node;

typedef struct Node Node;

struct Node {
    int value;
    Node *next;
};

typedef struct LinkedList {
    Node *head;
} LinkedList;

void ll_init(LinkedList *l) {
    l->head = NULL;
}

Node *n_init(int value) {
    Node *t = malloc(sizeof(*t));
    t->value = value;
    t->next  = NULL;
    return t;
}

void ll_insert(LinkedList *l, int value) {
    Node **t = &l->head;

    if (!*t) {
        *t = n_init(value);
        return;
    }

    while((*t)->next != NULL)
        t = &(*t)->next;

    (*t)->next = n_init(value);
}

void ll_print(LinkedList *l) {
    Node *t = l->head;

    while (t) {
        printf("%d -> ", t->value);
        t = t->next;
    }

    printf("x \n");
}

int ll_delete(LinkedList *l, int index) {
    Node *curr = l->head;
    Node *prev = NULL;
    Node *next = l->head->next;

    for (int i = 0; (i < index); i++) {
        prev = curr;
        curr = curr->next;
        if (!curr)
            break;
        next = curr->next;
    }

    if (!curr) // Null?
        return -1;

    free(curr);

    if (prev)
        prev->next = next;

    if (index == 0)
        l->head = next;

    return 1;
}

void ll_free(LinkedList *l) {
    Node *curr = l->head;
    Node *next;

    while (curr) {
        next = curr->next;
        free(curr);
        curr = next;
    }

    l->head = NULL;
}

/******************************************************************************/


/**************************** SHARED VARIABLES ********************************/


LinkedList list;

int next = 0;
int iter = 0;

#define MODE_SEARCH 2
#define MODE_INSERT 1
#define MODE_DELETE 0

int current_mode = -1;
int active = 0;

pthread_mutex_t write_mutex, mode_mutex;
int active_searchers;

int running_threads = 0;

/******************************************************************************/

/******************************** FUNCTIONS ***********************************/

void *searcher(void *n)
{
    int num = *((int *) n);
    int lim;
    next = 0;

    MSGS(num, "I am a searcher\n");
    pthread_mutex_lock(&mode_mutex);
    if (current_mode == MODE_INSERT)
        MSGS(num, "Seems current mode is insert.\n");
    else if (current_mode == MODE_SEARCH)
        MSGS(num, "Seems current mode is search.\n");
    else if (current_mode == MODE_DELETE) {
        MSGS(num, "Seems current mode is delete. Exiting.\n");
        pthread_mutex_unlock(&mode_mutex);
        running_threads--;
        return NULL;
    } else {
        MSGS(num, "Unknown Mode. Exiting.\n");
        pthread_mutex_unlock(&mode_mutex);
        running_threads--;
        return NULL;
    }

    if (!active) {
        MSGS(num, "But it's not active yet.\n");
        current_mode = MODE_SEARCH;
        active = 1;
    }
    active_searchers++;
    pthread_mutex_unlock(&mode_mutex);

    lim = rand() % iter;
    MSGS(num, "Searching for Element Number %d.\n", lim);
    Node *t = list.head;

    for (int i = 0; i < lim && t; i++) {
        t = t->next;
    }

    if (!t)
        MSGS(num, "Element Number %d not in list\n", lim);
    else
        MSGS(num, "Element Number %d is %d\n", lim, t->value);

    MSGS(num, "Exiting\n");

    pthread_mutex_lock(&mode_mutex);
    active_searchers--;
    running_threads--;
    if (current_mode == MODE_SEARCH)
        active = 0;
    pthread_mutex_unlock(&mode_mutex);
}

void *deleter(void *n)
{
    int num = *((int *) n);
    int index;
    next = 0;

    MSGD(num, "I am an Deleter\n");

    pthread_mutex_lock(&mode_mutex);
    if ((current_mode == MODE_SEARCH || current_mode == MODE_INSERT) && active) {
        MSGD(num, "List either in search or insert mode. Exiting.\n");
        pthread_mutex_unlock(&mode_mutex);
        running_threads--;
        return NULL;
    }
    current_mode = MODE_DELETE;
    active = 1;
    pthread_mutex_unlock(&mode_mutex);

    pthread_mutex_lock(&write_mutex);
    index = rand() % iter;
    MSGD(num, "Deleting Element Number %d.\n", index);
    if (ll_delete(&list, index) < 0)
        MSGD(num, "Element Number %d not in list\n", index);
    else
        MSGD(num, "Element Number %d Deleted\n", index);
    ll_print(&list);
    pthread_mutex_unlock(&write_mutex);

    MSGD(num, "Done. Exiting\n");

    pthread_mutex_lock(&mode_mutex);
    active = 0;
    running_threads--;
    pthread_mutex_unlock(&mode_mutex);
}

void *inserter(void *n)
{
    int num = *((int *) n);
    next = 0;

    MSGI(num, "I am an Inserter\n");
    pthread_mutex_lock(&mode_mutex);
    current_mode = MODE_INSERT;
    active = 1;
    pthread_mutex_unlock(&mode_mutex);

    pthread_mutex_lock(&write_mutex);
    MSGI(num, "Inserting %d\n", iter);
    ll_insert(&list, iter);
    iter += 1;
    ll_print(&list);
    MSGI(num, "Waiting for searchers to finish up...\n");
    while (active_searchers > 0);
    pthread_mutex_unlock(&write_mutex);
    MSGI(num, "Done. Exiting\n");

    pthread_mutex_lock(&mode_mutex);
    active = 0;
    running_threads--;
    pthread_mutex_unlock(&mode_mutex);
}

/******************************************************************************/

int main()
{
    int ret, nsearcher, ninserter, ndeleter;
    srand(1234);
    ll_init(&list);
    pthread_t inserter_threads[10], deleter_threads[10], searcher_threads[10];
    pthread_mutex_init(&write_mutex, NULL);
    pthread_mutex_init(&mode_mutex, NULL);

    printf("Enter number of inserters: ");
    scanf("%d", &ninserter);
    printf("Enter number of searchers: ");
    scanf("%d", &nsearcher);
    printf("Enter number of deleter: ");
    scanf("%d", &ndeleter);

    MSGM("Starting Threads\n");
    for (int i = 0; i < ninserter; i++) {
        ret = pthread_create(&inserter_threads[i], NULL, inserter, (void *) &i);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating inserter\n");
            return -1;
        }
        running_threads++;
    }

    for (int i = 0; i < nsearcher; i++) {
        ret = pthread_create(&searcher_threads[i], NULL, searcher, (void *) &i);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating searcher\n");
            return -1;
        }
        running_threads++;
    }

    for (int i = 0; i < ndeleter; i++) {
        ret = pthread_create(&deleter_threads[i], NULL, deleter, (void *) &i);
        next = 1;
        while (next);
        if (ret) {
            MSGM("Error in creating deleter\n");
            return -1;
        }
        running_threads++;
    }


    while (running_threads > 0);
    MSGM("All threads exited. Exiting\n");
    printf("Final list:\n");
    ll_print(&list);
    pthread_exit(NULL);
    ll_free(&list);
    return 0;
}
