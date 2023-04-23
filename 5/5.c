#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SINGLE_ROOMS 10
#define DOUBLE_ROOMS 15
#define TOTAL_ROOMS (SINGLE_ROOMS + DOUBLE_ROOMS)

typedef struct {
    int A[10];
    int B[15];
    int done;
    int single_rooms;
    int double_rooms;
    sem_t sem_mutex;
    sem_t sem_done;
} hotel_status;

static hotel_status *hotel;
static bool should_exit = false;

void signal_handler(int signal) { should_exit = true; }

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

    close(shm_fd);
}

void init_semaphores() {
    sem_init(&hotel->sem_mutex, 1, 1);
    sem_init(&hotel->sem_done, 1, 1);
}

void destroy_shared_memory() {
    munmap(hotel, sizeof(hotel_status));
    shm_unlink("hotel_shm");
}

void destroy_semaphores() {
    sem_destroy(&hotel->sem_mutex);
    sem_destroy(&hotel->sem_done);
}

void customer(bool is_male) {
    int iteraion = 0;
    while (!should_exit) {
        if (iteraion++ == 3) {
            printf("Посетитель устал ходить по кругу.\n");
            sem_wait(&hotel->sem_done);
            hotel->done++;
            sem_post(&hotel->sem_done);
            break;
        }
        usleep(rand() % 2000000 + 500000);

        if (is_male) {
            sem_wait(&hotel->sem_mutex);

            int flag = -1;
            for (int i = 0; i < 10; ++i) {
                if (hotel->A[i] == 0) {
                    hotel->A[i] = 1;
                    flag = i;
                    break;
                }
            }

            if (flag != -1) {
                hotel->single_rooms--;
                printf("Мужчина заселяется в одноместный номер. Оставшиеся номера: %d "
                       "одноместных, %d двухместных\n",
                       hotel->single_rooms, hotel->double_rooms);
                sem_post(&hotel->sem_mutex);
                usleep(rand() % 2000000 + 500000);
                sem_wait(&hotel->sem_mutex);
                hotel->single_rooms++;
                printf("Мужчина освобождает одноместный номер.\n");
                hotel->A[flag] = 0;
                sem_post(&hotel->sem_mutex);
                continue;
            }

            for (int i = 0; i < 15; ++i) {
                if (hotel->B[i] == 0 || hotel->B[i] == 1) {
                    hotel->B[i]++;
                    flag = i;
                    break;
                }
            }

            if (flag != -1) {
                if (hotel->B[flag] == 1) {
                    hotel->double_rooms--;
                }
                printf("Мужчина заселяется в двухместный номер. Оставшиеся номера: %d "
                       "одноместных, %d двухместных\n",
                       hotel->single_rooms, hotel->double_rooms);
                sem_post(&hotel->sem_mutex);
                usleep(rand() % 2000000 + 500000);

                sem_wait(&hotel->sem_mutex);
                printf("Мужчина освобождает двухместный номер.\n");
                hotel->B[flag]--;
                if (hotel->B[flag] == 0) {
                    hotel->double_rooms++;
                }
                sem_post(&hotel->sem_mutex);
                continue;
            }

            printf("Нет свободных номеров. Мужчина уходит.\n");
            sem_post(&hotel->sem_mutex);

        } else {
            sem_wait(&hotel->sem_mutex);

            int flag = -1;
            for (int i = 0; i < 10; ++i) {
                if (hotel->A[i] == 0) {
                    hotel->A[i] = 1;
                    flag = i;
                    break;
                }
            }

            if (flag != -1) {
                hotel->single_rooms--;
                printf("Женщина заселяется в одноместный номер. Оставшиеся номера: %d "
                       "одноместных, %d двухместных\n",
                       hotel->single_rooms, hotel->double_rooms);
                sem_post(&hotel->sem_mutex);
                usleep(rand() % 2000000 + 500000);
                sem_wait(&hotel->sem_mutex);
                hotel->single_rooms++;
                printf("Женщина освобождает одноместный номер.\n");
                hotel->A[flag] = 0;
                sem_post(&hotel->sem_mutex);
                continue;
            }

            for (int i = 0; i < 15; ++i) {
                if (hotel->B[i] == 0 || hotel->B[i] == -1) {
                    hotel->B[i]--;
                    flag = i;
                    break;
                }
            }

            if (flag != -1) {
                if (hotel->B[flag] == -1) {
                    hotel->double_rooms--;
                }
                printf("Женщина заселяется в двухместный номер. Оставшиеся номера: %d "
                       "одноместных, %d двухместных\n",
                       hotel->single_rooms, hotel->double_rooms);
                sem_post(&hotel->sem_mutex);
                usleep(rand() % 2000000 + 500000);

                sem_wait(&hotel->sem_mutex);
                printf("Женщина освобождает двухместный номер.\n");
                hotel->B[flag]++;
                if (hotel->B[flag] == 0) {
                    hotel->double_rooms++;
                }
                sem_post(&hotel->sem_mutex);
                continue;
            }

            printf("Нет свободных номеров. Женщина уходит.\n");
            sem_post(&hotel->sem_mutex);
        }
    }
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
    init_semaphores();

    for (int i = 0; i < 10; ++i) {
        hotel->A[i] = 0;
    }
    for (int i = 0; i < 15; ++i) {
        hotel->B[i] = 0;
    }

    hotel->done = 0;
    hotel->single_rooms = SINGLE_ROOMS;
    hotel->double_rooms = DOUBLE_ROOMS;

    pid_t *pids = malloc(num_clients * sizeof(pid_t));

    for (int i = 0; i < num_clients; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            srand(getpid());
            customer(i % 2 == 0);
            exit(0);
        }
    }

    while (!should_exit) {
        usleep(5000);
        sem_wait(&hotel->sem_done);
        if (hotel->done == num_clients) {
            break;
        }
        sem_post(&hotel->sem_done);
    }

    for (int i = 0; i < num_clients; i++) {
        kill(pids[i], SIGINT);
    }

    for (int i = 0; i < num_clients; i++) {
        wait(NULL);
    }

    free(pids);
    destroy_shared_memory();
    destroy_semaphores();

    return 0;
}
