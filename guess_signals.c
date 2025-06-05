#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>

volatile sig_atomic_t start_signal_received = 0;

void sig_start_handler(int sig, siginfo_t *info, void *ucontext) {
    start_signal_received = 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s N\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int N = atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "N должно быть положительным числом\n");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL) ^ getpid());

    int total_rounds = 10; // минимум 10 раундов

    // Создаем второй процесс
    pid_t child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // Блокируем сигналы
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    sigaddset(&mask, SIGRTMIN+1);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    // Обработчик для старта угадывающего
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sig_start_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN+1, &sa, NULL);

    // Параметры таймаута для sigtimedwait
    struct timespec timeout = {5, 0}; // 5 секунд

    for (int round = 1; round <= total_rounds; round++) {
        int is_parent = (child_pid > 0);
        int secret_holder = is_parent ? (round % 2 == 1) : (round % 2 == 0);

        if (secret_holder) {
            int secret = (rand() % N) + 1;
            printf("Раунд %d: Процесс %d (ЗАГАДЫВАЮЩИЙ) загадывает число.\n", round, getpid());
            fflush(stdout);

            struct timeval start, end;
            gettimeofday(&start, NULL);
            int attempts = 0;
            int guessed = 0;

            pid_t other_pid = is_parent ? child_pid : getppid();
            kill(other_pid, SIGRTMIN+1);

            while (!guessed) {
                siginfo_t si;
                int sig = sigtimedwait(&mask, &si, &timeout);
                if (sig == -1) {
                    if (errno == EAGAIN) {
                        // тайм-аут, продолжаем ждать
                        continue;
                    } else {
                        perror("sigtimedwait");
                        exit(EXIT_FAILURE);
                    }
                }
                if (sig == SIGRTMIN) {
                    int guess = si.si_value.sival_int;
                    attempts++;
                    printf("Раунд %d: Процесс %d (ЗАГАДЫВАЮЩИЙ) получил догадку: %d\n", round, getpid(), guess);
                    fflush(stdout);
                    if (guess == secret) {
                        kill(other_pid, SIGUSR1);
                        guessed = 1;
                    } else {
                        kill(other_pid, SIGUSR2);
                    }
                }
            }

            gettimeofday(&end, NULL);
            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
            printf("Раунд %d: Процесс %d (ЗАГАДЫВАЮЩИЙ) завершил раунд. Попыток: %d, время: %.3f сек.\n",
                   round, getpid(), attempts, elapsed);
            fflush(stdout);

        } else {
            // Ожидаем сигнала старта от загадывающего
            while (!start_signal_received) {
                sigsuspend(&oldmask);
            }
            start_signal_received = 0;  // сбрасываем флаг
            printf("Раунд %d: Процесс %d (УГАДЫВАЮЩИЙ) начинает угадывать.\n", round, getpid());
            fflush(stdout);

            struct timeval start, end;
            gettimeofday(&start, NULL);
            int attempts = 0;
            int guess = 0;
            int guessed = 0;
            pid_t other_pid = is_parent ? child_pid : getppid();

            while (!guessed) {
                guess++; // последовательный перебор от 1
                attempts++;
                union sigval value;
                value.sival_int = guess;
                sigqueue(other_pid, SIGRTMIN, value);
                printf("Раунд %d: Процесс %d (УГАДЫВАЮЩИЙ) отправил догадку: %d\n", round, getpid(), guess);
                fflush(stdout);

                // Ожидаем ответа: SIGUSR1 (верно) или SIGUSR2 (неверно)
                siginfo_t si;
                int sig = sigtimedwait(&mask, &si, &timeout);
                if (sig == -1) {
                    if (errno == EAGAIN) {
                        continue;
                    } else {
                        perror("sigtimedwait");
                        exit(EXIT_FAILURE);
                    }
                }
                if (sig == SIGUSR1) {
                    guessed = 1;
                    printf("Раунд %d: Процесс %d (УГАДЫВАЮЩИЙ) получил верный ответ для догадки %d.\n", round, getpid(), guess);
                    fflush(stdout);
                }
            }

            gettimeofday(&end, NULL);
            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
            printf("Раунд %d: Процесс %d (УГАДЫВАЮЩИЙ) завершил раунд. Попыток: %d, время: %.3f сек.\n",
                   round, getpid(), attempts, elapsed);
            fflush(stdout);
        }
    }

    if (child_pid > 0) {
        wait(NULL);
    }
    return 0;
}
