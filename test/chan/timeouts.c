#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <cee/cee.h>
#include <cee/chan.h>

CHAN_DEF(int);
CHAN_DEF_P(chan(int));

#define THREADC 16

void *
adder(void *arg) {
    chan(int) *c = (chan(int) *)arg;
    int i;
    long long sum = 0;
    while (chan_recv(c, &i) != CHAN_CLOSED) {
        sum += i;
        usleep(1000);
    }
    long long *ret = malloc(sizeof(*ret));
    *ret = sum;
    printf("%llu\n", sum);
    return ret;
}

void *
identity(void *arg) {
    chan(p(chan(int))) *c = (chan(p(chan(int))) *)arg;
    chan(int) *chani;
    assert(chan_recv(c, &chani) == CHAN_OK);
    int id;
    assert(chan_recv(chani, &id) == CHAN_OK);
    while (chan_send(chani, id) != CHAN_CLOSED) {
        usleep(1000);
    }
    return NULL;
}

int
main(void) {
    srand(time(NULL));

    chan(int) *c = chan_make(int, 1);
    pthread_t pool[THREADC];
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, c) == 0);
    }
    int ok = 0, timedout = 0;
    for (int i = 1; i <= 10000; i++) {
        if (chan_timedsend(c, i, 100) == CHAN_OK) {
            ok++;
        } else {
            timedout++;
        }
    }
    chan_close(c);
    long long sum = 0;
    long long *r;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("ok: %d timedout: %d\n", ok, timedout);
    printf("%llu\n", sum);
    c = chan_drop(c);

    c = chan_make(int, 0);
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, c) == 0);
    }
    ok = 0, timedout = 0;
    for (int i = 1; i <= 10000; i++) {
        if (chan_timedsend(c, i, 100) == CHAN_OK) {
            ok++;
        } else {
            timedout++;
        }
    }
    chan_close(c);
    sum = 0;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("ok: %d timedout: %d\n", ok, timedout);
    printf("%llu\n", sum);
    c = chan_drop(c);

    chan(int) *cpool[THREADC];
    chan(p(chan(int))) *c1 = chan_make(p(chan(int)), 0);
    int ir;
    int stats[THREADC] = {0};
    chan_case cases[THREADC];
    for (int i = 0; i < THREADC; i++) {
        cpool[i] = chan_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, c1) == 0);
        assert(chan_send(c1, cpool[i]) == CHAN_OK);
        assert(chan_send(cpool[i], i) == CHAN_OK);
        cases[i] = chan_case(cpool[i], CHAN_RECV, &ir);
    }
    ok = timedout = 0;
    for (int i = 1; i <= 10000; i++) {
        size_t id = chan_timedselect(cases, THREADC, 100);
        if (id != CHAN_WBLOCK) {
            ok++;
            assert((unsigned)ir == id);
            stats[ir]++;
        } else {
            timedout++;
        }
    }
    for (int i = 0; i < THREADC; i++) {
        chan_close(cpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        cpool[i] = chan_drop(cpool[i]);
        printf("%d: %d ", i, stats[i]);
    }
    c1 = chan_drop(c1);
    printf("\b\nok: %d timedout: %d\n", ok, timedout);

    printf("All tests passed\n");
    return 0;
}
