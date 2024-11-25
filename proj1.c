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

int pipe_fd[2];
pthread_mutex_t mutex;
int thread_sums[NUM_THREADS_CHILD];
volatile sig_atomic_t start_child_threads = 0;

void signalHandler(int sig) {
    if (sig == SIGUSR1) {
        start_child_threads = 1;
    }
}

void* readAndSum(void* arg) {
    int threadID = *((int*)arg);
    free(arg);

    int sum = 0, number;

    for (int i = 0; i < NUM_NUMBERS_PER_THREAD; i++) {
        pthread_mutex_lock(&mutex);
        if (read(pipe_fd[0], &number, sizeof(int)) > 0) {
            sum += number;
        } else {
            perror("Child thread: Error reading from pipe");
        }
        pthread_mutex_unlock(&mutex);
    }

    thread_sums[threadID] = sum;
    pthread_exit(NULL);
}

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

        printf("Child process: Waiting for signal from parent...\n");

        while (!start_child_threads)
            pause();

        printf("Child process: Signal received. Starting threads.\n");

        for (i = 0; i < NUM_THREADS_CHILD; i++) {
            int* thread_id = malloc(sizeof(int));
            if (thread_id == NULL) {
                perror("Child: Memory allocation failed");
                exit(EXIT_FAILURE);
            }
            *thread_id = i;

            if (pthread_create(&child_threads[i], NULL, readAndSum, thread_id) != 0) {
                perror("Child: Thread creation failed");
                free(thread_id);
                exit(EXIT_FAILURE);
            }
        }

        for (i = 0; i < NUM_THREADS_CHILD; i++) {
            if (pthread_join(child_threads[i], NULL) != 0) {
                perror("Child: Thread join failed");
                exit(EXIT_FAILURE);
            }
        }

        int total = 0;
        for (i = 0; i < NUM_THREADS_CHILD; i++) {
            total += thread_sums[i];
        }
        double average = (double)total / NUM_THREADS_CHILD;

        printf("Child process: Average of sums = %.2f\n", average);

        FILE* output_file = freopen("average_result.txt", "w", stdout);
        if (output_file == NULL) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }

        printf("Average of sums: %.2f\n", average);
        fclose(output_file);

        close(pipe_fd[0]);
    }

    pthread_mutex_destroy(&mutex);

    return 0;
}
