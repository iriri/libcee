/* Shamelessly copied from babby's first Go program. Yes, it's slower than
 * trial division, yes it leaks, kind of. */
#include <pthread.h>
#include <stdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cee/chan.h>

CHAN_DEF(int);

#define THREADC 512

void *
generate(void *arg) {
    chan(int) *c = (chan(int) *)arg;
    for (int i = 2; chan_send(c, i) == CHAN_OK; i++);
    return NULL;
}

void *
filter(void *args) {
    chan(int) *in = ((chan(int) **)args)[0];
    chan(int) *out = ((chan(int) **)args)[1];
    int p = ((intptr_t *)args)[2];
    free(args);

    int i;
    while(chan_recv(in, &i) == CHAN_OK) {
        if (i % p != 0) {
            chan_send(out, i);
        }
    }
    return NULL;
}

int
main(void) {
    chan(int) *c = chan_make(int, 0), *c1;
    pthread_t gen, fltrpool[THREADC];
    pthread_create(&gen, NULL, generate, c);

    int p = 0;
    for (int i = 0; i < THREADC; i++) {
        chan_recv(c, &p);
        printf("%d\n", p);

        void **args = malloc(3 * sizeof(*args));
        args[0] = c;
        args[1] = (c1 = chan_make(int, 0));
        args[2] = (void *)(intptr_t)p;

        pthread_create(fltrpool + i, NULL, filter, args);
        c = c1;
    }
    return 0;
}
