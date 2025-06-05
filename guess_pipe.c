#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>

#define BUF_SIZE 16

void write_int(int fd, int value) {
    if(write(fd, &value, sizeof(value)) != sizeof(value)) {
        perror("write_int");
        exit(EXIT_FAILURE);
    }
}

int read_int(int fd) {
    int value;
    ssize_t n = read(fd, &value, sizeof(value));
    if(n == 0) {
        exit(EXIT_SUCCESS);
    } else if(n != sizeof(value)) {
        perror("read_int");
        exit(EXIT_FAILURE);
    }
    return value;
}

void write_start(int fd) {
    const char *msg = "START";
    if(write(fd, msg, strlen(msg)) != (ssize_t)strlen(msg)) {
        perror("write_start");
        exit(EXIT_FAILURE);
    }
}

void read_start(int fd) {
    char buf[BUF_SIZE] = {0};
    ssize_t n = read(fd, buf, strlen("START"));
    if(n <= 0) {
        perror("read_start");
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0';
    if(strcmp(buf, "START") != 0) {
        fprintf(stderr, "Ожидалось START, получено: %s\n", buf);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Использование: %s N\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int N = atoi(argv[1]);
    if(N <= 0) {
        fprintf(stderr, "N должно быть положительным числом\n");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL) ^ getpid());
    int total_rounds = 10;

    int pipe_parent_to_child[2], pipe_child_to_parent[2];
    if(pipe(pipe_parent_to_child) < 0 || pipe(pipe_child_to_parent) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t child_pid = fork();
    if(child_pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    int round;
    for(round = 1; round <= total_rounds; round++) {
        int is_parent = (child_pid > 0);
        int secret_holder;
        if(is_parent) {
            secret_holder = (round % 2 == 1);
        } else {
            secret_holder = (round % 2 == 0);
        }

        if(secret_holder) {
            // Процесс загадывает число
            int secret = (rand() % N) + 1;
            printf("Раунд %d: Процесс %d (ЗАГАДЫВАЮЩИЙ) загадывает число.\n", round, getpid());
            fflush(stdout);
            struct timeval start, end;
            gettimeofday(&start, NULL);
            int attempts = 0;

            if(is_parent) {
                write_start(pipe_parent_to_child[1]);
                while(1) {
                    // Читаем догадку от ребёнка из pipe_child_to_parent
                    int guess = read_int(pipe_child_to_parent[0]);
                    attempts++;
                    printf("Раунд %d: Процесс %d (ЗАГАДЫВАЮЩИЙ) получил догадку: %d\n", round, getpid(), guess);
                    fflush(stdout);
                    if(guess == secret) {
                        // Отправляем ответ 1 (верно) через pipe_parent_to_child
                        write_int(pipe_parent_to_child[1], 1);
                        break;
                    } else {
                        // Отправляем ответ 0 (неверно)
                        write_int(pipe_parent_to_child[1], 0);
                    }
                }
            } else {
                write_start(pipe_child_to_parent[1]);
                while(1) {
                    int guess = read_int(pipe_parent_to_child[0]);
                    attempts++;
                    printf("Раунд %d: Процесс %d (ЗАГАДЫВАЮЩИЙ) получил догадку: %d\n", round, getpid(), guess);
                    fflush(stdout);
                    if(guess == secret) {
                        write_int(pipe_child_to_parent[1], 1);
                        break;
                    } else {
                        write_int(pipe_child_to_parent[1], 0);
                    }
                }
            }
            gettimeofday(&end, NULL);
            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec)/1000000.0;
            printf("Раунд %d: Процесс %d (ЗАГАДЫВАЮЩИЙ) завершил раунд. Попыток: %d, время: %.3f сек.\n",
                   round, getpid(), attempts, elapsed);
            fflush(stdout);
        } else {
            struct timeval start, end;
            gettimeofday(&start, NULL);
            int attempts = 0;
            int guess = 0;
            if(is_parent) {
                read_start(pipe_child_to_parent[0]);
                while(1) {
                    guess++;  // перебор от 1
                    attempts++;
                    write_int(pipe_parent_to_child[1], guess);
                    printf("Раунд %d: Процесс %d (УГАДЫВАЮЩИЙ) отправил догадку: %d\n", round, getpid(), guess);
                    fflush(stdout);
                    int resp = read_int(pipe_child_to_parent[0]);
                    if(resp == 1) {
                        printf("Раунд %d: Процесс %d (УГАДЫВАЮЩИЙ) получил верный ответ для догадки %d.\n", round, getpid(), guess);
                        fflush(stdout);
                        break;
                    }
                }
            } else {
                read_start(pipe_parent_to_child[0]);
                while(1) {
                    guess++;
                    attempts++;
                    write_int(pipe_child_to_parent[1], guess);
                    printf("Раунд %d: Процесс %d (УГАДЫВАЮЩИЙ) отправил догадку: %d\n", round, getpid(), guess);
                    fflush(stdout);
                    int resp = read_int(pipe_parent_to_child[0]);
                    if(resp == 1) {
                        printf("Раунд %d: Процесс %d (УГАДЫВАЮЩИЙ) получил верный ответ для догадки %d.\n", round, getpid(), guess);
                        fflush(stdout);
                        break;
                    }
                }
            }
            gettimeofday(&end, NULL);
            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec)/1000000.0;
            printf("Раунд %d: Процесс %d (УГАДЫВАЮЩИЙ) завершил раунд. Попыток: %d, время: %.3f сек.\n",
                   round, getpid(), attempts, elapsed);
            fflush(stdout);
        }
    }

    // Закрываем каналы и ожидаем завершения дочернего процесса (если родитель)
    if(child_pid > 0) {
        close(pipe_parent_to_child[0]);
        close(pipe_parent_to_child[1]);
        close(pipe_child_to_parent[0]);
        close(pipe_child_to_parent[1]);
        wait(NULL);
    }
    return 0;
}
