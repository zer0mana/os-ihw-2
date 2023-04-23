#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
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
} hotel_status;

static hotel_status *hotel;
static int sem_id;
static bool should_exit = false;

void signal_handler(int signal) { should_exit = true; }

void init_shared_memory() {
    int shm_id = shmget(IPC_PRIVATE, sizeof(hotel_status), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(1);
    }

    hotel = (hotel_status *)shmat(shm_id, NULL, 0);
    if (hotel == (hotel_status *)-1) {
        perror("shmat");
        exit(1);
    }

    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }
}

void init_semaphores() {
    sem_id = semget(IPC_PRIVATE, 2, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }

    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } sem_union;

    unsigned short sem_values[2] = {1, 1};
    sem_union.array = sem_values;

    if (semctl(sem_id, 0, SETALL, sem_union) == -1) {
        perror("semctl");
        exit(1);
    }
}

void destroy_shared_memory() {
    shmdt(hotel);
}

void destroy_semaphores() {
    if (semctl(sem_id, 0, IPC_RMID, NULL) == -1) {
        perror("semctl");
        exit(1);
    }
}

void sem_wait(int sem_num) {
    struct sembuf sem_op = {sem_num, -1, 0};
    if (semop(sem_id, &sem_op, 1) == -1) {
        perror("semop");
        exit(1);
    }
}

void sem_post(int sem_num) {
    struct sembuf sem_op = {sem_num, 1, 0};
    if (semop(sem_id, &sem_op, 1) == -1) {
        perror("semop");
        exit(1);
    }
}

void customer(bool is_male) {
    int iteraion = 0;
    while (!should_exit) {
        if (iteraion++ == 3) {
            printf("Посетитель устал ходить по кругу.\n");
            sem_wait(0);
            hotel->done++;
            sem_post(0);
            break;
        }
        usleep(rand() % 2000000 + 500000);

        if (is_male) {
            sem_wait(1);

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
                sem_post(1);
                usleep(rand() % 2000000 + 500000);
                sem_wait(1);
                hotel->single_rooms++;
                printf("Мужчина освобождает одноместный номер.\n");
                hotel->A[flag] = 0;
                sem_post(1);
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
                sem_post(1);
                usleep(rand() % 2000000 + 500000);

                sem_wait(1);
                printf("Мужчина освобождает двухместный номер.\n");
                hotel->B[flag]--;
                if (hotel->B[flag] == 0) {
                    hotel->double_rooms++;
                }
                sem_post(1);
                continue;
            }

            printf("Нет свободных номеров. Мужчина уходит.\n");
            sem_post(1);

        } else {
            sem_wait(1);

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
                sem_post(1);
                usleep(rand() % 2000000 + 500000);
                sem_wait(1);
                hotel->single_rooms++;
                printf("Женщина освобождает одноместный номер.\n");
                hotel->A[flag] = 0;
                sem_post(1);
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
                sem_post(1);
                usleep(rand() % 2000000 + 500000);

                sem_wait(1);
                printf("Женщина освобождает двухместный номер.\n");
                hotel->B[flag]++;
                if (hotel->B[flag] == 0) {
                    hotel->double_rooms++;
                }
                sem_post(1);
                continue;
            }

            printf("Нет свободных номеров. Женщина уходит.\n");
            sem_post(1);
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
        sem_wait(0);
        if (hotel->done == num_clients) {
            break;
        }
        sem_post(0);
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
