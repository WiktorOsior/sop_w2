#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))


#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
(__extension__                                                              \
({ long int __result;                                                     \
do __result = (long int) (expression);                                 \
while (__result == -1L && errno == EINTR);                             \
__result; }))
#endif

volatile sig_atomic_t sigint = 0;
pthread_mutex_t sigmtx = PTHREAD_MUTEX_INITIALIZER;

volatile sig_atomic_t sigalrm = 0;
pthread_mutex_t sigalrmmtx = PTHREAD_MUTEX_INITIALIZER;

volatile sig_atomic_t sigusr1 = 0;
pthread_mutex_t sigusr1mtx = PTHREAD_MUTEX_INITIALIZER;

typedef struct product {
    pthread_mutex_t mx;
    int value;
} product_t;

typedef struct context {
    int idx;
    product_t* shelf;
    int size;
    unsigned seed;
}context_t;

typedef struct context_signal {
    sigset_t oldmask;
    product_t* shelf;
    int size;
} context_signal_t;

void signal_handler(int i) {
    pthread_mutex_lock(&sigmtx);
    sigint = 1;
    pthread_mutex_unlock(&sigmtx);
}

void signal_handler_alrm(int i) {
    pthread_mutex_lock(&sigalrmmtx);
    sigalrm = 1;
    pthread_mutex_unlock(&sigalrmmtx);
}

void signal_handler_usr1(int i) {
    pthread_mutex_lock(&sigusr1mtx);
    sigusr1 = 1;
    pthread_mutex_unlock(&sigusr1mtx);
}

void* work(void* args){
    context_t* ctx = (context_t *)args;
    int i1, i2;
  printf("Worker %lu reporting \n",(unsigned long)pthread_self());
    while (1) {
        pthread_mutex_lock(&sigmtx);
        int sigint_cp = sigint;
        pthread_mutex_unlock(&sigmtx);

        if (sigint_cp) break;

        i1 = rand_r(&ctx->seed) % ctx->size;
        do {
            i2 = rand_r(&ctx->seed) % ctx->size;
        }while (i1 == i2);
        if (i1 > i2) {
            int temp = i1;
            i1 = i2;
            i2 = temp;
        }

        pthread_mutex_lock(&ctx->shelf[i1].mx);
        pthread_mutex_lock(&ctx->shelf[i2].mx);

        if (ctx->shelf[i1].value > ctx->shelf[i2].value) {
            int temp = ctx->shelf[i1].value;
            ctx->shelf[i1].value = ctx->shelf[i2].value;
            ctx->shelf[i2].value = temp;
        }

        pthread_mutex_unlock(&ctx->shelf[i1].mx);
        pthread_mutex_unlock(&ctx->shelf[i2].mx);
    }
    free(ctx);
    return NULL;
  }

void shufle(product_t * prod, int size){
    srand(getpid());
    for (int i = 0; i < size; i++){
        pthread_mutex_init(&prod[i].mx,NULL);
        prod[i].value = rand() % size;
    }
}

void print(product_t* shelf, int size) {
    for (int i = 0; i < size; i++){
        printf("%d ", shelf[i].value);
    }
    printf("\n");
}

void* signaler_work (void* args) {
    context_signal_t * ctx = (context_signal_t*)args;

    alarm(1);

    while (1) {
        sigsuspend(&ctx->oldmask);
        if (sigint) break;

        if (sigalrm) {
            for (int i = 0; i < ctx->size; i++) {
                pthread_mutex_lock(&ctx->shelf[i].mx);
            }
            for (int i = 0; i < ctx->size; i++){
                printf("%d ",ctx->shelf[i].value);
            }
            printf("\n");
            for (int i = 0; i < ctx->size; i++) {
                pthread_mutex_unlock(&ctx->shelf[i].mx);
            }
            sigalrm = 0;
            alarm(1);
        }
        if (sigusr1) {
            srand(getpid());
            for (int i = 0; i < ctx->size; i++) {
                pthread_mutex_lock(&ctx->shelf[i].mx);
            }
            for (int i = 0; i < ctx->size; i++){
                ctx->shelf[i].value = rand() % ctx->size;
                printf("%d ",ctx->shelf[i].value);
            }
            printf("\n");
            for (int i = 0; i < ctx->size; i++) {
                pthread_mutex_unlock(&ctx->shelf[i].mx);
            }
            sigusr1 = 0;
        }
    }
    free(ctx);
    return NULL;
}

int main(int argc,char **argv)
{
    printf("%d\n",getpid());
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT,&sa,NULL);

    sa.sa_handler = signal_handler_alrm;
    sigaction(SIGALRM,&sa,NULL);

    sa.sa_handler = signal_handler_usr1;
    sigaction(SIGUSR1,&sa,NULL);

    int err;
    sigset_t s, oldmask;
    sigemptyset(&s);
    sigaddset(&s, SIGINT);
    sigaddset(&s, SIGALRM);
    sigaddset(&s, SIGUSR1);

    if ((err = pthread_sigmask(SIG_BLOCK, &s, &oldmask)) != 0) {
        fprintf(stderr, "pthread_sigmask(): %s\n", strerror(err));
        return 1;
    }

    if(argc < 3) ERR("not eough arguments");
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    pthread_t * threads = malloc(sizeof(pthread_t) * M);

    product_t * shelf = malloc(sizeof(product_t) * N);
    shufle(shelf,N);
    print(shelf,N);
    

    for(int i = 0; i < M; i++)
    {
        context_t* ctx = malloc(sizeof(context_t));
        ctx->idx = i;
        ctx->shelf = shelf;
        ctx->size = N;
        pthread_create(&threads[i],NULL,work,ctx);
    }

    pthread_t signaler;
    context_signal_t* ctx = malloc(sizeof(context_signal_t));
    ctx->oldmask = oldmask;
    ctx->shelf = shelf;
    ctx->size = N;
    pthread_create(&signaler,NULL,signaler_work,ctx);
    pthread_join(signaler,NULL);


    for(int i = 0; i < M; i++)
    {
        pthread_join(threads[i],NULL);
    }

    print(shelf,N);
    free(threads);
    free(shelf);
    return 0;
}

