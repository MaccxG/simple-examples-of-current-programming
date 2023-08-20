/**
 * A simple example of the bounded-buffer problem, with n producers and m consumers, 
 * both given as input when invoke the program, solved using condition variables.
 * The buffer is managed via a circular array and it's size is 10, the number of elements
 * to be produced and consumed is 100.
 * After the consumer has withdrawn an element, a neutral value must be placed in that position.
 * The program ends when all the elements have been produced and consumed, at the end the buffer
 * is empty.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 10
#define NEUTRAL_VALUE 0
#define items_to_produce 100
#define items_to_consume 100

typedef struct {
    int buffer[BUFFER_SIZE];
    int in;
    int out;
    int produced_items;
    int consumed_items;

    pthread_mutex_t mutex;
    pthread_cond_t empty;
    pthread_cond_t full;
    int current_items_num;
} shared_data;

typedef struct {
    pthread_t tid;
    int thread_i;
    
    shared_data *shared;
} producer_data;

typedef struct {
    pthread_t tid;
    int thread_i;
    
    shared_data *shared;
} consumer_data;

void init_shared(shared_data *shared) {
    for (int i = 0; i < BUFFER_SIZE; i++)
        shared->buffer[i] = NEUTRAL_VALUE;

    shared->in = shared->out = 0;

    shared->produced_items = shared->consumed_items = 0;

    shared->current_items_num = 0;

    int err;
    if ((err = pthread_mutex_init(&shared->mutex, NULL)) != 0) {
        fprintf(stderr, "Error in pthread_mutex_init: %d\n", err);
        return;
    }
    if ((err = pthread_cond_init(&shared->empty, NULL)) != 0) {
        fprintf(stderr, "Error in pthread_cond_init: %d\n", err);
        return;        
    }
    if ((err = pthread_cond_init(&shared->full, NULL)) != 0) {
        fprintf(stderr, "Error in pthread_cond_init: %d\n", err);
        return;           
    }
}

void destroy_shared(shared_data *shared) {
    pthread_mutex_destroy(&shared->mutex);
    pthread_cond_destroy(&shared->empty);
    pthread_cond_destroy(&shared->full);
    free(shared);
}

// prints the current buffer state
void printBuffer(int *buffer) {
    for (int i = 0; i < BUFFER_SIZE; i++)
        printf("%d ", buffer[i]);
    printf("\n\n");
}

void producer(void *arg) {
    producer_data *prod_data = (producer_data *)arg;
    int data;
    int err;

    while (prod_data->shared->produced_items < items_to_produce) {
        data = rand() % 99 + 1;

        if ((err = pthread_mutex_lock(&prod_data->shared->mutex)) != 0)
            fprintf(stderr, "Error in pthread_mutex_lock: %d\n", err);

        while (prod_data->shared->current_items_num == BUFFER_SIZE) {
            if ((err = pthread_cond_wait(&prod_data->shared->full, &prod_data->shared->mutex)) != 0)
                fprintf(stderr, "Error in pthread_cond_wait: %d\n", err);
        }

        if (prod_data->shared->produced_items < items_to_produce) {  // needed for the last few threads lagged behind
            prod_data->shared->buffer[prod_data->shared->in] = data;
            printf("P%d: buffer[%d] = %d\n", prod_data->thread_i, prod_data->shared->in, data);

            prod_data->shared->in = (prod_data->shared->in + 1) % BUFFER_SIZE;
            prod_data->shared->produced_items++;

            prod_data->shared->current_items_num++;

            printBuffer(prod_data->shared->buffer);
        }
        
        if ((err = pthread_cond_signal(&prod_data->shared->empty)) != 0)
            fprintf(stderr, "Error in pthread_cond_signal: %d\n", err);

        if ((err = pthread_mutex_unlock(&prod_data->shared->mutex)) != 0)
            fprintf(stderr, "Error in pthread_mutex_unlock: %d\n", err);
    }
}

void consumer(void *arg) {
    consumer_data *cons_data = (consumer_data *)arg;
    int data;
    int err;

    while (cons_data->shared->consumed_items < items_to_consume) {
        if ((err = pthread_mutex_lock(&cons_data->shared->mutex)) != 0)
            fprintf(stderr, "Error in pthread_mutex_lock: %d\n", err);

        while (cons_data->shared->current_items_num == 0 && cons_data->shared->consumed_items != items_to_consume) {
            if ((err = pthread_cond_wait(&cons_data->shared->empty, &cons_data->shared->mutex)) != 0)
                fprintf(stderr, "Error in pthread_cond_wait: %d\n", err);            
        }

        if (cons_data->shared->consumed_items < items_to_consume) { // needed for the last few threads lagged behind
            data = cons_data->shared->buffer[cons_data->shared->out];
            printf("C%d: buffer[%d] = %d\n", cons_data->thread_i, cons_data->shared->out, data);

            cons_data->shared->buffer[cons_data->shared->out] = NEUTRAL_VALUE;
            cons_data->shared->out = (cons_data->shared->out + 1) % BUFFER_SIZE;
            cons_data->shared->consumed_items++;

            cons_data->shared->current_items_num--;

            printBuffer(cons_data->shared->buffer);
        }

        if ((err = pthread_cond_signal(&cons_data->shared->full)) != 0)
            fprintf(stderr, "Error in pthread_cond_signal: %d\n", err);

        if ((err = pthread_mutex_unlock(&cons_data->shared->mutex)) != 0)
            fprintf(stderr, "Error in pthread_mutex_unlock: %d\n", err);
    }
}

int main(int argc, char **argv) {
    // check parameters number
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <number of producers> <number of consumers>\n", argv[0]);
        exit(1);
    }

    char *str_end1, *str_end2;
    int  producers_num = (int)strtol(argv[1], &str_end1, 10);
    int consumers_num = (int)strtol(argv[2], &str_end2, 10);

    // check parameters
    if ((*str_end1 != '\0' || producers_num <= 0) || (*str_end2 != '\0' || consumers_num <= 0)) {
        fprintf(stderr, "Invalid number of producers and consumers.\n");
        exit(1);
    }

    shared_data *shared = malloc(sizeof(shared_data));
    producer_data prod_data[producers_num];
    consumer_data cons_data[consumers_num];
    int err;

    init_shared(shared);

    // create producers
    srand(time(NULL));
    for (int i = 0; i < producers_num; i++) {
        prod_data[i].thread_i = i + 1;
        prod_data[i].shared = shared;
        if ((err = pthread_create(&prod_data[i].tid, NULL, (void *)producer, &prod_data[i])) != 0) {
            fprintf(stderr, "Error in pthread_create: %d\n", err);
            exit(1);
        }
    }

    // create consumers
    for (int i = 0; i < consumers_num; i++) {
        cons_data[i].thread_i = i + 1;
        cons_data[i].shared = shared;
        if ((err = pthread_create(&cons_data[i].tid, NULL, (void *)consumer, &cons_data[i])) != 0) {
            fprintf(stderr, "Error in pthread_create: %d\n", err);
            exit(1);
        }
    }

    // waiting for the producers to terminate 
    for (int i = 0; i < producers_num; i++) {
        if ((err = pthread_join(prod_data[i].tid, NULL)) != 0) {
            fprintf(stderr, "Error in pthread_join: %d\n", err);
            exit(1);            
        }
    }

    // waiting for the consumers to terminate 
    for (int i = 0; i < consumers_num; i++) {
        if ((err = pthread_join(cons_data[i].tid, NULL)) != 0) {
            fprintf(stderr, "Error in pthread_join: %d\n", err);
            exit(1);            
        }
    }

    destroy_shared(shared);

    exit(0);
}