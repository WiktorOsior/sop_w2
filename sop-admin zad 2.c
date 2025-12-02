#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

/* --- DOSTARCZONE MAKRA I FUNKCJE POMOCNICZE --- */
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

#define ERR(source) \
    (kill(0, SIGKILL), perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

/* Zmienne globalne do obsługi sygnałów */
volatile sig_atomic_t last_signal_pid = 0;
volatile sig_atomic_t sig_usr1_received = 0;
volatile sig_atomic_t sig_alarm_received = 0;
volatile sig_atomic_t sig_term_received = 0;

/* Handler dla SIGUSR1 (Kaszlnięcie) - pobiera PID nadawcy */
void action_handler(int sig, siginfo_t *info, void *context) {
    sig_usr1_received = 1;
    last_signal_pid = info->si_pid;
}

/* Handler dla SIGALRM i SIGTERM */
void basic_handler(int sig) {
    if (sig == SIGALRM) sig_alarm_received = 1;
    if (sig == SIGTERM) sig_term_received = 1;
}

void set_action_handler(int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = action_handler;
    act.sa_flags = SA_SIGINFO;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void set_basic_handler(int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = basic_handler;
    // Brak SA_RESTART kluczowy dla przerywania wait() i nanosleep()
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

typedef struct {
    pid_t pid;
    int status_type; // 0 - parents picked up, 1 - stayed
    int coughs;
} ChildResult;

void child_work(int k, int p_prob, int is_ill) {
    pid_t my_pid = getpid();
    int cough_count = 0;

    // Maski
    sigset_t mask, oldmask, waitmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    waitmask = oldmask; // Maska do czekania (odblokowane sygnały)

    printf("Child[%d] starts day in the kindergarten, ill: %d\n", my_pid, is_ill);

    if (is_ill) alarm(k);

    while (1) {
        if (sig_term_received) {
            printf("Child[%d] exits with %d\n", my_pid, cough_count);
            exit(cough_count);
        }
        if (sig_alarm_received) {
            printf("Child[%d] exits with %d\n", my_pid, cough_count);
            exit(cough_count);
        }

        if (sig_usr1_received) {
            sig_usr1_received = 0;
            if (last_signal_pid != my_pid) {
                printf("Child[%d]: %d has coughed at me!\n", my_pid, last_signal_pid);
                if (!is_ill) {
                    if ((rand() % 100) < p_prob) {
                        is_ill = 1;
                        printf("Child[%d] get sick!\n", my_pid);
                        alarm(k);
                    }
                }
            }
        }

        if (is_ill) {
            // Chory śpi losowo 50-200ms
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = (50 + rand() % 151) * 1000000L;

            // Atomowe odblokowanie i sen
            sigprocmask(SIG_SETMASK, &waitmask, NULL);
            nanosleep(&ts, NULL);
            sigprocmask(SIG_BLOCK, &mask, NULL);

            if (!sig_term_received && !sig_alarm_received) {
                cough_count++;
                printf("Child[%d] is coughing (%d)\n", my_pid, cough_count);
                kill(0, SIGUSR1);
            }
        } else {
            // Zdrowy czeka na sygnał
            sigsuspend(&waitmask);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s t k n p\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int t = atoi(argv[1]);
    int k = atoi(argv[2]);
    int n = atoi(argv[3]);
    int p_prob = atoi(argv[4]);

    set_action_handler(SIGUSR1);
    set_basic_handler(SIGALRM);
    set_basic_handler(SIGTERM); // Rodzic może zignorować SIGTERM, ale dzieci muszą obsłużyć

    ChildResult *results = malloc(sizeof(ChildResult) * n);
    if (!results) ERR("malloc");

    pid_t parent_pid = getpid();

    // Tworzenie grupy procesów (rodzic staje się liderem grupy)
    // Dzieki temu kill(0, ...) zadziała na rodzica i dzieci
    setpgid(0, 0);

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) ERR("fork");
        if (pid == 0) {
            srand(time(NULL) ^ (getpid() << 16));
            child_work(k, p_prob, (i == 0) ? 1 : 0);
            exit(EXIT_SUCCESS);
        }
        results[i].pid = pid;
        results[i].status_type = -1;
    }

    printf("KG[%d]: Alarm has been set for %d sec\n", parent_pid, t);
    alarm(t);

    int children_left = n;
    int simulation_ended = 0;

    while (children_left > 0) {
        if (sig_alarm_received) {
            sig_alarm_received = 0;
            if (!simulation_ended) {
                printf("KG[%d]: Simulation has ended!\n", parent_pid);
                simulation_ended = 1;
                // Zabijamy tylko te dzieci, które jeszcze są
                for (int i = 0; i < n; i++) {
                     if (results[i].status_type == -1)
                         kill(results[i].pid, SIGTERM);
                }
            }
        }

        if (sig_usr1_received) sig_usr1_received = 0; // Rodzic ignoruje kaszel

        int status;
        pid_t p = waitpid(-1, &status, 0); // Blocking wait
        if (p > 0) {
            for (int i = 0; i < n; i++) {
                if (results[i].pid == p) {
                    if (WIFEXITED(status)) {
                        results[i].coughs = WEXITSTATUS(status);
                        // Jeśli symulacja się skończyła (wysłaliśmy SIGTERM),
                        // to dziecko zostało w przedszkolu.
                        // Jeśli symulacja trwa (simulation_ended == 0),
                        // to dziecko wyszło wcześniej (rodzice).
                        results[i].status_type = simulation_ended ? 1 : 0;
                    }
                    break;
                }
            }
            children_left--;
        } else {
            if (errno == EINTR) continue; // Przerwane przez sygnał, sprawdź flagi
            if (errno == ECHILD) break;
            ERR("waitpid");
        }
    }

    // Wyświetlanie tabeli
    printf("No. | Child ID | Status\n");
    int stayed_count = 0;
    for (int i = 0; i < n; i++) {
        printf("%3d |    %5d | Coughed %d times and ", i + 1, results[i].pid, results[i].coughs);
        if (results[i].status_type == 0) {
            printf("parents picked them up!\n");
        } else {
            printf("is still in the kindergarten!\n");
            stayed_count++;
        }
    }
    printf("%d out of %d children stayed in the kindergarten!\n", stayed_count, n);

    free(results);
    return 0;
}