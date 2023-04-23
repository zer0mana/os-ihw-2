#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

static bool should_exit = false;

void signal_handler(int signal) { should_exit = true; }

#define SINGLE_ROOMS 10
#define DOUBLE_ROOMS 15
#define TOTAL_ROOMS (SINGLE_ROOMS + DOUBLE_ROOMS)

typedef struct {
    int single_rooms;
    int double_rooms;
    int single_occupants;
    int double_occupants;
    int A[10];
    int B[15];
    int done;
} hotel_status;

static hotel_status *hotel;

void init_shared_memory() {
    int shm_fd = shm_open("hotel_shm", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, sizeof(hotel_status)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    hotel = (hotel_status *)mmap(NULL, sizeof(hotel_status),
                                 PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (hotel == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    for (int i = 0; i < 10; ++i) {
        hotel->A[i] = 0;
    }

    for (int i = 0; i < 15; ++i) {
        hotel->B[i] = 0;
    }

    hotel->single_rooms = SINGLE_ROOMS;
    hotel->double_rooms = DOUBLE_ROOMS;
    hotel->single_occupants = 0;
    hotel->double_occupants = 0;
    hotel->done = 0;

    close(shm_fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_clients>\n", argv[0]);
        exit(1);
    }

    int num_clients = atoi(argv[1]);

    if (num_clients < 1) {
        fprintf(stderr, "Number of clients must be greater than 0\n");
        exit(1);
    }

    signal(SIGINT, signal_handler);

    init_shared_memory();
    sem_t *sem_mutex = sem_open("sem_mutex", O_CREAT, 0666, 1);

    pid_t *pids = malloc(num_clients * sizeof(pid_t));

    for (int i = 0; i < num_clients; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            char is_male_str[2];
            sprintf(is_male_str, "%d", i % 2 == 0);
            execl("./client", "./client", is_male_str, (char *)NULL);
            perror("execl");
            exit(1);
        }
    }

    while (!should_exit) {
        usleep(500000);
        sem_wait(sem_mutex);
        if (hotel->done == num_clients) {
            break;
        }
        sem_post(sem_mutex);
    }

    for (int i = 0; i < num_clients; i++) {
        kill(pids[i], SIGINT);
    }

    for (int i = 0; i < num_clients; i++) {
        wait(NULL);
    }

    wait(NULL);

    free(pids);

    return 0;
}
