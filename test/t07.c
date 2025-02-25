#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define NUM_THREADS 5
#define INCREMENTS_PER_THREAD 1000000

// Shared counter
int counter = 0;
// Mutex for thread synchronization
pthread_mutex_t mutex;

void* increment_counter(void* arg) {
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        pthread_mutex_lock(&mutex); // Lock the mutex
        counter++; // Critical section
        pthread_mutex_unlock(&mutex); // Unlock the mutex
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    pthread_mutex_init(&mutex, NULL); // Initialize the mutex

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, increment_counter, NULL) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    // Join threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex); // Destroy the mutex

    // Print the final counter value
    printf("Final Counter Value: %d\n", counter);
    return 0;
}
