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

    chan(int) *chanpool[THREADC];
    chan(p(chan(int))) *c3 = chan_make(p(chan(int)), 0);
    for (int i = 0; i < THREADC; i++) {
        chanpool[i] = chan_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, c3) == 0);
        assert(chan_send(c3, chanpool[i]) == CHAN_OK);
        assert(chan_send(chanpool[i], i) == CHAN_OK);
    }
    int ir;
    for (int i = 1; i <= 100; i++) {
        chan_poll(THREADC) {
            chan_case(0, chan_tryrecv(chanpool[0], &ir), assert(ir == 0));
            chan_case(1, chan_tryrecv(chanpool[1], &ir), { // Also works
                    printf("1, %d\n", ir);
                    assert(ir == 1);
            });
            chan_case(2, chan_tryrecv(chanpool[2], &ir), assert(ir == 2));
            chan_case(3, chan_tryrecv(chanpool[3], &ir), assert(ir == 3));
            chan_case(4, chan_tryrecv(chanpool[4], &ir), assert(ir == 4));
            chan_case(5, chan_tryrecv(chanpool[5], &ir), assert(ir == 5));
            chan_case(6, chan_tryrecv(chanpool[6], &ir), assert(ir == 6));
            chan_case(7, chan_tryrecv(chanpool[7], &ir), assert(ir == 7));
            chan_case(8, chan_tryrecv(chanpool[8], &ir), assert(ir == 8));
            chan_case(9, chan_tryrecv(chanpool[9], &ir), assert(ir == 9));
            chan_case(10, chan_tryrecv(chanpool[10], &ir), assert(ir == 10));
            chan_case(11, chan_tryrecv(chanpool[11], &ir), assert(ir == 11));
            chan_case(12, chan_tryrecv(chanpool[12], &ir), assert(ir == 12));
            chan_case(13, chan_tryrecv(chanpool[13], &ir), assert(ir == 13));
            chan_case(14, chan_tryrecv(chanpool[14], &ir), assert(ir == 14));
            chan_case(15, chan_tryrecv(chanpool[15], &ir), assert(ir == 15));
            chan_default({
                printf("default\n");
            });
        } chan_poll_end;
    }
    for (int i = 0; i < THREADC; i++) {
        chan_close(chanpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        chanpool[i] = chan_drop(chanpool[i]);
    }
    c3 = chan_drop(c3);

    printf("All tests passed\n");
    return 0;
}
