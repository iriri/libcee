#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#include <cee/chan.h>

#define THREADC 16
#define LIM 1000000ll

CHAN_DEF(long);

void *
sender(void *arg) {
    chan(long) *c = (chan(long) *)arg;
    for (long i = 1; i <= LIM; i++) {
        chan_send(c, i);
    }
    c = chan_close(c);
    return NULL;
}

void *
receiver(void *arg) {
    chan(long) *c = (chan(long) *)arg;
    long i;
    long long sum = 0;
    while (chan_recv(c, &i) != CHAN_CLOSED) {
        sum += i;
    }
    c = chan_drop(c);
    printf("%lld\n", sum);
    return (void *)sum;
}

int
main(void) {
    chan(long) *c = chan_make(long, 8);
    pthread_t senders[THREADC];
    pthread_t recvers[THREADC];
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(
            senders + i, NULL, sender, chan_open(c)) == 0);
    }
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_create(recvers + i, NULL, receiver, chan_dup(c)) == 0);
    }
    chan_close(c);
    c = chan_drop(c);

    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(senders[i], NULL) == 0);
    }
    long long sum = 0, t = 0;
    for (int i = 0; i < THREADC; i++) {
        assert(pthread_join(recvers[i], (void **)&t) == 0);
        sum += t;
    }
    printf("%lld\n", sum);
    assert(sum == ((LIM * (LIM + 1))/2) * THREADC);
    return 0;
}
