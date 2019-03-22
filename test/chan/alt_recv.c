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
identity(void *arg) {
    chan(p(chan(int))) *c = (chan(p(chan(int))) *)arg;
    chan(int) *c1;
    assert(chan_recv(c, &c1) == CHAN_OK);
    int id;
    assert(chan_recv(c1, &id) == CHAN_OK);
    while (chan_send(c1, id) != CHAN_CLOSED) {
        usleep(10000);
    }
    return NULL;
}

int
main(void) {
    srand(time(NULL));

    chan(int) *cpool[THREADC];
    pthread_t pool[THREADC];
    chan(p(chan(int))) *c = chan_make(p(chan(int)), 0);
    int ir;
    chan_case cases[THREADC];
    for (int i = 0; i < THREADC; i++) {
        cpool[i] = chan_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, c) == 0);
        assert(chan_send(c, cpool[i]) == CHAN_OK);
        assert(chan_send(cpool[i], i) == CHAN_OK);
        cases[i] = chan_case(cpool[i], CHAN_RECV, &ir);
    }
    for (int i = 1; i <= 1000; i++) {
        int id = chan_alt(cases, THREADC);
        printf("%d, %d\n", id, ir);
        if (ir != id) {
            printf("FUG: %d %d\n", ir, id);
        }
        assert(ir == id);
    }
    for (int i = 0; i < THREADC; i++) {
        chan_close(cpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        cpool[i] = chan_drop(cpool[i]);
    }
    c = chan_drop(c);

    printf("All tests passed\n");
    return 0;
}
