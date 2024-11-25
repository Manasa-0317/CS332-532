#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define NUM_THREADS_PARENT 3
#define NUM_RANDOM_NUMBERS 500
#define NUM_THREADS_CHILD 10
#define NUM_NUMBERS_PER_THREAD 150

void* generateRandomNumbers(void* arg) {
    int threadID = *((int*)arg);
    free(arg);

    unsigned int seed = time(NULL) + threadID;

    for (int i = 0; i < NUM_RANDOM_NUMBERS; i++) {
        int randomNumber = rand_r(&seed) % 1001;

        pthread_mutex_lock(&mutex);
        if (write(pipe_fd[1], &randomNumber, sizeof(int)) == -1) {
            perror("Parent thread: Error writing to pipe");
            pthread_mutex_unlock(&mutex);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&mutex);
    }

    pthread_exit(NULL);
}

int main(void) {
    pthread_t parent_threads[NUM_THREADS_PARENT];
    pthread_t child_threads[NUM_THREADS_CHILD];
    pid_t pid;
    int i;

    pthread_mutex_init(&mutex, NULL);

    if (pipe(pipe_fd) == -1) {
        perror("Pipe creation failed");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        close(pipe_fd[0]);

        for (i = 0; i < NUM_THREADS_PARENT; i++) {
            int* thread_id = malloc(sizeof(int));
            if (thread_id == NULL) {
                perror("Parent: Memory allocation failed");
                exit(EXIT_FAILURE);
            }
            *thread_id = i;

            if (pthread_create(&parent_threads[i], NULL, generateRandomNumbers, thread_id) != 0) {
                perror("Parent: Thread creation failed");
                free(thread_id);
                exit(EXIT_FAILURE);
            }
        }

        for (i = 0; i < NUM_THREADS_PARENT; i++) {
            if (pthread_join(parent_threads[i], NULL) != 0) {
                perror("Parent: Thread join failed");
                exit(EXIT_FAILURE);
            }
        }

        printf("Parent process: All threads completed execution.\n");

        kill(pid, SIGUSR1);

        close(pipe_fd[1]);

    } else {
        close(pipe_fd[1]);

        signal(SIGUSR1, signalHandler);

        close(pipe_fd[0]);
    }

    pthread_mutex_destroy(&mutex);

    return 0;
}
