#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <cee/chan.h>

CHAN_DEF(int);
CHAN_DEF_P(chan(int));

#define THREADC 16

void *
identity(void *arg) {
    chan(p(chan(int))) *c = (chan(p(chan(int))) *)arg;
    chan(int) *ci;
    assert(chan_recv(c, &ci) == CHAN_OK);
    int id, ir;
    assert(chan_recv(ci, &id) == CHAN_OK);
    while (chan_recv(ci, &ir) != CHAN_CLOSED) {
        printf("%d, %d\n", id, ir);
        assert(ir == id);
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
    int ia[THREADC];
    chan_set *set = chan_set_make(1);
    for (int i = 0; i < THREADC; i++) {
        cpool[i] = chan_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, c) == 0);
        assert(chan_send(c, cpool[i]) == CHAN_OK);
        assert(chan_send(cpool[i], i) == CHAN_OK);
        ia[i] = i;
        chan_set_add(set, cpool[i], CHAN_SEND, &ia[i]);
    }
    for (int i = 1; i <= 100; i++) {
        chan_select(set);
    }
    set = chan_set_drop(set);
    for (int i = 0; i < THREADC; i++) {
        chan_close(cpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        cpool[i] = chan_drop(cpool[i]);
    }
    c = chan_drop(c);

    printf("All tests passed\n");
    return 0;
}
