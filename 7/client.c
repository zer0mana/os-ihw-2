#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

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

void customer(bool is_male) {
    sem_t *sem_single = sem_open("sem_single", 0);
    sem_t *sem_double = sem_open("sem_double", 0);
    sem_t *sem_mutex = sem_open("sem_mutex", 0);

    int shm_fd = shm_open("hotel_shm", O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    hotel_status *hotel =
            (hotel_status *)mmap(NULL, sizeof(hotel_status), PROT_READ | PROT_WRITE,
                                 MAP_SHARED, shm_fd, 0);
    if (hotel == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    close(shm_fd);

    int iteraion = 0;
    while (1) {
        if (iteraion++ == 3) {
            printf("Посетитель устал ходить по кругу.\n");
            sem_wait(sem_mutex);
            hotel->done++;
            sem_post(sem_mutex);
            break;
        }
        usleep(rand() % 2000000 + 500000);

        if (is_male) {
            sem_wait(sem_mutex);

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
                sem_post(sem_mutex);
                usleep(rand() % 2000000 + 500000);
                sem_wait(sem_mutex);
                hotel->single_rooms++;
                printf("Мужчина освобождает одноместный номер.\n");
                hotel->A[flag] = 0;
                sem_post(sem_mutex);
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
                sem_post(sem_mutex);
                usleep(rand() % 2000000 + 500000);

                sem_wait(sem_mutex);
                printf("Мужчина освобождает двухместный номер.\n");
                hotel->B[flag]--;
                if (hotel->B[flag] == 0) {
                    hotel->double_rooms++;
                }
                sem_post(sem_mutex);
                continue;
            }

            printf("Нет свободных номеров. Мужчина уходит.\n");
            sem_post(sem_mutex);

        } else {
            sem_wait(sem_mutex);

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
                sem_post(sem_mutex);
                usleep(rand() % 2000000 + 500000);
                sem_wait(sem_mutex);
                hotel->single_rooms++;
                printf("Женщина освобождает одноместный номер.\n");
                hotel->A[flag] = 0;
                sem_post(sem_mutex);
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
                sem_post(sem_mutex);
                usleep(rand() % 2000000 + 500000);

                sem_wait(sem_mutex);
                printf("Женщина освобождает двухместный номер.\n");
                hotel->B[flag]++;
                if (hotel->B[flag] == 0) {
                    hotel->double_rooms++;
                }
                sem_post(sem_mutex);
                continue;
            }

            printf("Нет свободных номеров. Женщина уходит.\n");
            sem_post(sem_mutex);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <is_male (0 or 1)>\n", argv[0]);
        exit(1);
    }

    bool is_male = atoi(argv[1]) == 1;

    srand(getpid());
    customer(is_male);

    return 0;
}