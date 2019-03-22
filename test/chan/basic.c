#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "cee/chan.h"

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
    }
    long long *ret = malloc(sizeof(*ret));
    *ret = sum;
    printf("%llu\n", sum);
    return ret;
}

void *
identity(void *arg) {
    chan(p(chan(int))) *c = (chan(p(chan(int))) *)arg;
    chan(int) *c1;
    assert(chan_recv(c, &c1) == CHAN_OK);
    int id;
    assert(chan_recv(c1, &id) == CHAN_OK);
    while (chan_send(c1, id) != CHAN_CLOSED);
    return NULL;
}

chan_rc
forcesend(chan(int) *c, int i) {
    chan_rc rc;
    while ((rc = chan_trysend(c, i)) == CHAN_WBLOCK) {
        int j;
        chan_tryrecv(c, &j);
    }
    return rc;
}

int
main(void) {
    srand(time(NULL));

    int i;
    chan(int) *c = chan_make(int, 2);
    i = 1;
    assert(chan_send(c, i) == CHAN_OK);
    i = 2;
    assert(chan_send(c, i) == CHAN_OK);
    i = 3;
    assert(chan_trysend(c, i) == CHAN_WBLOCK);
    assert(forcesend(c, i) == CHAN_OK);
    assert(chan_tryrecv(c, &i) == CHAN_OK);
    assert(i == 2);
    assert(chan_recv(c, &i) == CHAN_OK);
    assert(i == 3);
    assert(chan_tryrecv(c, &i) == CHAN_WBLOCK);
    assert(i == 3);
    chan(int) *c1 = chan_open(c);
    chan_close(c1);
    chan(int) *c2 = chan_open(chan_dup(c1));
    chan_close(c);
    chan_close(c2);
    i = 1;
    assert(chan_send(c, i) == CHAN_CLOSED);
    c = chan_drop(c);
    assert(chan_send(c2, i) == CHAN_CLOSED);
    c2 = chan_drop(c2);

    c = chan_make(int, 1);
    pthread_t pool[THREADC];
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, c) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        chan_send(c, i);
    }
    chan_close(c);
    long long sum = 0;
    long long *r;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ll * 100001ll)/2));
    c = chan_drop(c);

    c = chan_make(int, 0);
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(pool + i, NULL, adder, c) == 0);
    }
    for (int i = 1; i <= 100000; i++) {
        assert(chan_send(c, i) == CHAN_OK);
    }
    chan_close(c);
    sum = 0;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(pool[i], (void **)&r) == 0);
        sum += *r;
        free(r);
    }
    printf("%llu\n", sum);
    assert(sum == ((100000ll * 100001ll)/2));
    c = chan_drop(c);

    chan(int) *cpool[THREADC];
    chan(p(chan(int))) *c3 = chan_make(p(chan(int)), 0);
    for (int i = 0; i < THREADC; i++) {
        cpool[i] = chan_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, c3) == 0);
        assert(chan_send(c3, cpool[i]) == CHAN_OK);
        assert(chan_send(cpool[i], i) == CHAN_OK);
    }
    int ir = 0;
    for (int i = 1; i <= 10000; i++) {
        chan_select(THREADC) {
        chan_case_recv(cpool[0], &ir, 0): assert(ir == 0);
        chan_case_recv(cpool[1], &ir, 1): assert(ir == 1);
        chan_case_recv(cpool[2], &ir, 2): assert(ir == 2);
        chan_case_recv(cpool[3], &ir, 3): assert(ir == 3);
        chan_case_recv(cpool[4], &ir, 4): assert(ir == 4);
        chan_case_recv(cpool[5], &ir, 5): assert(ir == 5);
        chan_case_recv(cpool[6], &ir, 6): assert(ir == 6);
        chan_case_recv(cpool[7], &ir, 7): assert(ir == 7);
        chan_case_recv(cpool[8], &ir, 8): assert(ir == 8);
        chan_case_recv(cpool[9], &ir, 9): assert(ir == 9);
        chan_case_recv(cpool[10], &ir, 10): assert(ir == 10);
        chan_case_recv(cpool[11], &ir, 11): assert(ir == 11);
        chan_case_recv(cpool[12], &ir, 12): assert(ir == 12);
        chan_case_recv(cpool[13], &ir, 13): assert(ir == 13);
        chan_case_recv(cpool[14], &ir, 14): assert(ir == 14);
        chan_case_recv(cpool[15], &ir, 15): assert(ir == 15);
        chan_case_closed(): assert(false);
        } chan_select_end;
    }
    for (int i = 0; i < THREADC; i++) {
        chan_close(cpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        cpool[i] = chan_drop(cpool[i]);
    }
    c3 = chan_drop(c3);

    printf("All tests passed\n");
    return 0;
}
