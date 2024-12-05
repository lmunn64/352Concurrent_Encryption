#include <stdio.h>
#include "encrypt-module.h"
#include "circular-queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

int inputSize = -1;
int outputSize = -1;
c_queue *input_queue;
c_queue *output_queue;

bool reset_request = false;

pthread_mutex_t input_mutex;
pthread_mutex_t output_mutex;
pthread_mutex_t reset_mutex;

// Conditions for input queue
pthread_cond_t input_read_cond;   // Reader can read
pthread_cond_t input_consume_cond; // Encryptor can consume
pthread_cond_t input_count_cond;  // Input counter can count

// Conditions for output queue
pthread_cond_t output_read_cond;  // Encryptor can read
pthread_cond_t output_consume_cond; // Writer can consume
pthread_cond_t output_count_cond; // Output counter can count

// Conditions for reset
pthread_cond_t reset_complete_cond;
pthread_cond_t io_equal_cond;

void print_counts() {
    printf("Total input count with current key is %d\n", get_input_total_count());
    for (char i = 'A'; i <= 'Z'; i++) {
        printf("%c:%d ", i, get_input_count(i));
    }
    printf("\n");
    printf("Total output count with current key is %d\n", get_output_total_count());
    for (char i = 'A'; i <= 'Z'; i++) {
        printf("%c:%d ", i, get_output_count(i));
    }
    printf("\n");
}

void reset_requested() {
    pthread_mutex_lock(&reset_mutex);
    reset_request = true;
    while (get_input_total_count() != get_output_total_count()) {
        pthread_cond_wait(&io_equal_cond, &reset_mutex);
    }
    pthread_mutex_unlock(&reset_mutex);
    print_counts();
}

void reset_finished() {
    pthread_mutex_lock(&reset_mutex);
    reset_request = false;
    pthread_cond_signal(&reset_complete_cond);
    pthread_mutex_unlock(&reset_mutex);
    printf("Reset finished.\n");
}

void *writer(void *arg) {
    while (1) {
        pthread_mutex_lock(&output_mutex);
        while (!can_consume(output_queue)) {
            pthread_cond_wait(&output_consume_cond, &output_mutex);
        }
        char output = consume(output_queue);
        if (output == EOF) {
            pthread_mutex_unlock(&output_mutex);
            break;
        }
        write_output(output);
        pthread_cond_signal(&output_read_cond);
        pthread_mutex_unlock(&output_mutex);
    }
    return NULL;
}

void *reader(void *arg) {
    char c;
    while ((c = read_input()) != EOF) {
        pthread_mutex_lock(&input_mutex);
        while (!can_read(input_queue)) {
            pthread_cond_wait(&input_read_cond, &input_mutex);
        }
        while (reset_request) {
            pthread_cond_wait(&reset_complete_cond, &input_mutex);
        }
        read(input_queue, c);
        pthread_cond_signal(&input_consume_cond);
        pthread_cond_signal(&input_count_cond);
        pthread_mutex_unlock(&input_mutex);
    }
    pthread_mutex_lock(&input_mutex);
    read(input_queue, EOF);
    pthread_cond_signal(&input_consume_cond);
    pthread_cond_signal(&input_count_cond);
    pthread_mutex_unlock(&input_mutex);
    return NULL;
}

void *encryptor(void *arg) {
    while (1) {
        pthread_mutex_lock(&input_mutex);
        while (!can_consume(input_queue)) {
            pthread_cond_wait(&input_consume_cond, &input_mutex);
        }
        char to_encrypt = consume(input_queue);
        pthread_cond_signal(&input_read_cond);
        pthread_mutex_unlock(&input_mutex);
        if (to_encrypt == EOF) {
            pthread_mutex_lock(&output_mutex);
            read(output_queue, EOF);
            pthread_cond_signal(&output_consume_cond);
            pthread_mutex_unlock(&output_mutex);
            break;
        }
        char encrypted = encrypt(to_encrypt);
        pthread_mutex_lock(&output_mutex);
        while (!can_read(output_queue)) {
            pthread_cond_wait(&output_read_cond, &output_mutex);
        }
        read(output_queue, encrypted);
        pthread_cond_signal(&output_consume_cond);
        pthread_cond_signal(&output_count_cond);
        pthread_mutex_unlock(&output_mutex);
    }
    return NULL;
}

void *input_counter(void *arg) {
    while (1) {
        pthread_mutex_lock(&input_mutex);
        while (!can_count(input_queue)) {
            pthread_cond_wait(&input_count_cond, &input_mutex);
        }
        char counted = count(input_queue);
        if (counted == EOF) {
            pthread_mutex_unlock(&input_mutex);
            break;
        }
        count_input(counted);
        pthread_mutex_unlock(&input_mutex);
    }
    return NULL;
}

void *output_counter(void *arg) {
    while (1) {
        pthread_mutex_lock(&output_mutex);
        while (!can_count(output_queue)) {
            pthread_cond_wait(&output_count_cond, &output_mutex);
        }
        char counted = count(output_queue);
        if (counted == EOF) {
            pthread_mutex_unlock(&output_mutex);
            break;
        }
        count_output(counted);
        if (get_input_total_count() == get_output_total_count()) {
            pthread_mutex_lock(&reset_mutex);
            pthread_cond_signal(&io_equal_cond);
            pthread_mutex_unlock(&reset_mutex);
        }
        pthread_mutex_unlock(&output_mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    char *in = argv[1];
    char *out = argv[2];
    char *log = argv[3]; // Assuming log is used elsewhere

    while (inputSize <= 0) {
        printf("Enter input buffer size (> 0): ");
        scanf("%d", &inputSize);
    }
    input_queue = new_c_queue(inputSize);

    while (outputSize <= 0) {
        printf("Enter output buffer size (> 0): ");
        scanf("%d", &outputSize);
    }
    output_queue = new_c_queue(outputSize);

    init(in, out);

    pthread_mutex_init(&input_mutex, NULL);
    pthread_mutex_init(&output_mutex, NULL);
    pthread_mutex_init(&reset_mutex, NULL);

    pthread_cond_init(&input_read_cond, NULL);
    pthread_cond_init(&input_consume_cond, NULL);
    pthread_cond_init(&input_count_cond, NULL);
    pthread_cond_init(&output_read_cond, NULL);
    pthread_cond_init(&output_consume_cond, NULL);
    pthread_cond_init(&output_count_cond, NULL);
    pthread_cond_init(&reset_complete_cond, NULL);
    pthread_cond_init(&io_equal_cond, NULL);

    pthread_t reader_thread, input_counter_thread, encryptor_thread, writer_thread, output_counter_thread;

    pthread_create(&reader_thread, NULL, reader, NULL);
    pthread_create(&input_counter_thread, NULL, input_counter, NULL);
    pthread_create(&encryptor_thread, NULL, encryptor, NULL);
    pthread_create(&writer_thread, NULL, writer, NULL);
    pthread_create(&output_counter_thread, NULL, output_counter, NULL);

    pthread_join(reader_thread, NULL);
    pthread_join(input_counter_thread, NULL);
    pthread_join(encryptor_thread, NULL);
    pthread_join(writer_thread, NULL);
    pthread_join(output_counter_thread, NULL);

    pthread_mutex_destroy(&input_mutex);
    pthread_mutex_destroy(&output_mutex);
    pthread_mutex_destroy(&reset_mutex);

    pthread_cond_destroy(&input_read_cond);
    pthread_cond_destroy(&input_consume_cond);
    pthread_cond_destroy(&input_count_cond);
    pthread_cond_destroy(&output_read_cond);
    pthread_cond_destroy(&output_consume_cond);
    pthread_cond_destroy(&output_count_cond);
    pthread_cond_destroy(&reset_complete_cond);
    pthread_cond_destroy(&io_equal_cond);

    printf("End of file reached.\n");
    print_counts();
    return 0;
}