#include "thread_pool.h"
#include "assert.h"

// The worker function to be executed by each thread in the thread pool.
static void *worker(void *arg) {
    TheadPool *tp = (TheadPool *)arg;  // Cast the argument to a ThreadPool pointer.
    while (true) {  // Infinite loop to keep the thread alive for processing tasks.
        pthread_mutex_lock(&tp->mu);  // Lock the mutex to safely access the queue.
        // Wait until the queue is not empty. If it is empty, the thread will be put to sleep.
        while (tp->queue.empty()) {
            pthread_cond_wait(&tp->not_empty, &tp->mu);  // Wait for a condition signal that the queue is not empty.
        }
        // A job is available. Get the work item from the front of the queue.
        Work w = tp->queue.front();
        tp->queue.pop_front();  // Remove the work item from the queue.
        pthread_mutex_unlock(&tp->mu);  // Unlock the mutex now that we're done with the queue.
        // Execute the work function with the provided argument.
        w.f(w.arg);
    }
    return NULL;  // This line is never reached due to the infinite loop, but it's here for completeness.
}

// Initialize the thread pool with a specified number of threads.
void thread_pool_init(TheadPool *tp, size_t num_threads) {
    assert(num_threads > 0);  // Ensure that at least one thread is requested.
    // Initialize the mutex.
    int rv = pthread_mutex_init(&tp->mu, NULL);
    assert(rv == 0);  // Check that the mutex was initialized successfully.
    // Initialize the condition variable.
    rv = pthread_cond_init(&tp->not_empty, NULL);
    assert(rv == 0);  // Check that the condition variable was initialized successfully.
    // Resize the thread vector to hold the desired number of threads.
    tp->threads.resize(num_threads);
    for (size_t i = 0; i < num_threads; i++) {
        // Create each thread and have it start running the worker function.
        int rv = pthread_create(&tp->threads[i], NULL, &worker, tp);
        assert(rv == 0);  // Ensure that the thread was created successfully.
    }
}

// Add a new work item to the thread pool's queue.
void thread_pool_queue(TheadPool *tp, void (*f)(void *), void *arg) {
    Work w;  // Create a new Work object.
    w.f = f;  // Set the function pointer to the provided function.
    w.arg = arg;  // Set the argument to be passed to the function.

    pthread_mutex_lock(&tp->mu);  // Lock the mutex to safely modify the queue.
    tp->queue.push_back(w);  // Add the new work item to the back of the queue.
    pthread_cond_signal(&tp->not_empty);  // Signal that the queue is not empty, waking up a worker thread.
    pthread_mutex_unlock(&tp->mu);  // Unlock the mutex.
}
