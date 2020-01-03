#include <assert.h>
#include <pthread.h>

#include <cee/mtx.h>

#define THREADC 64
#define LIM 100000ll

/* GCC seems to compile everything away if this isn't volatile. */
volatile long long sum;
mtx lock;

void *
adder(__attribute__((unused)) void *arg) {
    for (long long i = 0; i < LIM; i++) {
        mtx_lock(&lock);
        sum++;
        mtx_unlock(&lock);
    }
    return NULL;
}

int
main(void) {
    pthread_t pool[THREADC];
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, NULL) == 0);
    }
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], NULL) == 0);
    }
    assert(sum == LIM * THREADC);
    return 0;
}
