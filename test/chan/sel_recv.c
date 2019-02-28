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

    chan(int) *chanpool[THREADC];
    pthread_t pool[THREADC];
    chan(p(chan(int))) *c = chan_make(p(chan(int)), 0);
    int ir;
    chan_set *set = chan_set_make(THREADC / 2);
    for (int i = 0; i < THREADC; i++) {
        chanpool[i] = chan_make(int, 0);
        assert(pthread_create(pool + i, NULL, identity, c) == 0);
        assert(chan_send(c, chanpool[i]) == CHAN_OK);
        assert(chan_send(chanpool[i], i) == CHAN_OK);
        chan_set_add(set, chanpool[i], CHAN_RECV, &ir);
    }
    for (int i = 1; i <= 100; i++) {
        int id = chan_select(set);
        printf("%d, %d\n", id, ir);
        assert(ir == id);
    }
    set = chan_set_drop(set);
    for (int i = 0; i < THREADC; i++) {
        chan_close(chanpool[i]);
        assert(pthread_join(pool[i], NULL) == 0);
        chanpool[i] = chan_drop(chanpool[i]);
    }
    c = chan_drop(c);

    printf("All tests passed\n");
    return 0;
}
