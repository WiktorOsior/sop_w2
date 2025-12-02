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

#define FILE_MAX_SIZE 512

volatile sig_atomic_t sig_usr1 = 0;
volatile sig_atomic_t sig_usr2 = 0;
volatile sig_atomic_t sig_int = 0;

void usr1_handler(int sig) { sig_usr1 = 1; }
void usr2_handler(int sig) { sig_usr2 = 1; }
void int_handler(int sig) { sig_int = 1; }

ssize_t bulk_read(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  // EOF
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

void usage(int argc, char* argv[])
{
    printf("%s p h\n", argv[0]);
    printf("\tp - path to directory describing the structure of the Austro-Hungarian office in Prague.\n");
    printf("\th - Name of the highest administrator.\n");
    exit(EXIT_FAILURE);
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void ms_sleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (TEMP_FAILURE_RETRY(nanosleep(&ts, &ts)))
        ERR("nanosleep");
}

void child_work()
{
    int i = 0;
    pid_t pid = getpid();
    printf("Child %d starts\n",(int)pid);
    srand(pid);
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask,SIGUSR2);

    sigsuspend(&mask);
    if (sig_usr1)
    {
        sig_usr1 = 0;
        sig_usr2 = 0;
    }
    while (1)
    {
        while (!sig_usr2 && !sig_int)
        {
            ms_sleep(1000);
            ++i;
            printf("Child %d counter %d\n",pid,i);
        }
        if (sig_int)
        {
            char *filename = malloc(32);
            snprintf(filename, 32, "%d.txt", pid);
            char *in = malloc(32);
            int len = snprintf(in,32,"%d",i);
            printf("%s",in);
            int fd = open(filename,O_WRONLY | O_CREAT | O_TRUNC, 0644);
            write(fd,in,len);
            free(filename);
            close(fd);
            return;
        }
        sigsuspend(&mask);
        if (sig_usr1)
        {
            sig_usr1 = 0;
            sig_usr2 = 0;
        }
    }
}

int main(int argc, char* argv[])
{
    sethandler(usr1_handler, SIGUSR1);
    sethandler(usr2_handler, SIGUSR2);
    sethandler(int_handler, SIGINT);

    if (argc < 2)
    {
        ERR("no argument");
    }

    pid_t list[atoi(argv[1])];

    printf("Parent pid %d",getpid());

    for (int i = 0; i < atoi(argv[1]); i++)
    {
        printf("%d\n",i);
        pid_t child = fork();
        if (child == 0)
        {
            child_work();
            return EXIT_SUCCESS;
        }
        list[i] = child;
    }

    kill(list[0],SIGUSR1);

    int working = 0;

    sigset_t mask;
    sigemptyset(&mask);

    while (sigsuspend(&mask))
    {
        if (sig_int)
        {
            for (int i = 0; i < atoi(argv[1]); i++)
            {
                kill(list[i],SIGINT);
            }

            pid_t p;
            while ( (p = wait(NULL)) > 0)
            {
                printf("Child %d ended\n",(int)p);
            }
            break;
        }
        if (sig_usr2)
        {
            sig_usr2 = 0;
            kill(list[working],SIGUSR2);
            if (working == atoi(argv[1]) -1)
            {
                working = 0;
            }else
            {
                working++;
            }
            kill(list[working],SIGUSR1);
        }
    }

    return 0;

}
