#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include <cee/mtx.h>

/* GCC seems to compile everything away if this isn't volatile. */
struct {
    volatile size_t sum;
    unsigned char pad[64 - sizeof(size_t)];
    mtx lock;
} vars;

void *
adder(void *lim) {
    for (size_t i = 0; i < (size_t)lim; i++) {
        mtx_lock(&vars.lock);
        vars.sum++;
        mtx_unlock(&vars.lock);
    }
    return NULL;
}

struct {
    volatile size_t sum;
    unsigned char pad[64 - sizeof(size_t)];
    pthread_mutex_t lock;
} pthr_vars = {.lock = PTHREAD_MUTEX_INITIALIZER};

void *
pthr_adder(void *lim) {
    for (size_t i = 0; i < (size_t)lim; i++) {
        pthread_mutex_lock(&pthr_vars.lock);
        pthr_vars.sum++;
        pthread_mutex_unlock(&pthr_vars.lock);
    }
    return NULL;
}

clock_t
bench(size_t nthrs, size_t lim, void *(*fn)(void *)) {
    pthread_t pool[nthrs]; // vla gang vla gang vla gang vla gang
    clock_t begin = clock();
    for (size_t i = 0; i < nthrs; i++) {
        assert(pthread_create(pool + i, NULL, fn, (void *)lim) == 0);
    }
    for (size_t i = 0; i < nthrs; i++) {
        assert(pthread_join(pool[i], NULL) == 0);
    }
    return clock() - begin;
}

int
main(void) {
    struct {
        size_t nthrs, lim;
    } params[] = {
        {2, 0x800000},
        {4, 0x200000},
        {8, 0x100000},
        {16, 0x40000},
        {32, 0x40000},
        {64, 0x20000},
        {128, 0x8000},
    };
    for (size_t i = 0; i < sizeof(params) / sizeof(params[0]); i++) {
        vars.sum = 0;
        clock_t ticks = bench(params[i].nthrs, params[i].lim, adder);
        printf("mtx : %zu threads %ld ticks\n", params[i].nthrs, ticks);
        assert(vars.sum == params[i].lim * params[i].nthrs);

        pthr_vars.sum = 0;
        ticks = bench(params[i].nthrs, params[i].lim, pthr_adder);
        printf("pthr: %zu threads %ld ticks\n", params[i].nthrs, ticks);
        assert(pthr_vars.sum == params[i].lim * params[i].nthrs);

    }
    return 0;
}
