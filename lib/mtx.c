/* I didn't write this (completely) out of NIH symdrome. I just wanted a mutex
 * that didn't take up 5/8ths of a cache line, unlike glibc's. I ended up
 * shamelessly coping the implementation from "Mutexes and Condition Variables
 * using Futexes" by Steven Fuerst.
 *
 * Ironically the bloatedness of glibc's mutex can be a feature. Sometimes it
 * just happens to push the right fields into different cache lines, improving
 * performance by preventing false sharing. */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <cee/ftx.h>
#include <cee/xops.h>

#include <cee/mtx.h>

static const int SPINS = 64;
static const ftx UNLOCKED = 0x0000;
static const ftx LOCKED = 0x0001;
static const ftx LOCKED_CONTENDED = 0x0101;

void
mtx_lock_(mtx *m) {
    for (int i = 0, usec = 1; i < SPINS; i++) {
        uint8_t locked = 0;
        if (xcas_s_seq_acq(&m->_locked, &locked, true)) {
            return;
        }
        usec = ftx_backoff(usec);
    }

    while ((xchg_seq(&m->_state, LOCKED_CONTENDED) & LOCKED) != 0) {
        ftx_wait(&m->_state, LOCKED_CONTENDED);
    }
}

bool
mtx_trylock_(mtx *m) {
    uint8_t unlocked = 0;
    return xcas_s_seq_rlx(&m->_locked, &unlocked, true);
}

bool
mtx_timedlock_(mtx *m, const struct timespec *timeout) {
    for (int i = 0, usec = 1; i < SPINS; i++) {
        uint8_t locked = 0;
        if (xcas_s_seq_acq(&m->_locked, &locked, true)) {
            return true;
        }
        usec = ftx_backoff(usec);
    }

    int rc = 0;
    while (
        rc != ETIMEDOUT &&
        (xchg_seq(&m->_state, LOCKED_CONTENDED) & LOCKED) != 0
    ) {
        rc = ftx_timedwait(&m->_state, LOCKED_CONTENDED, timeout);
    }
    return rc != ETIMEDOUT;
}

void
mtx_unlock_(mtx *m) {
    if (xget_rlx(&m->_state) == LOCKED) {
        ftx locked = LOCKED;
        if (xcas_s_seq_rlx(&m->_state, &locked, UNLOCKED)) {
            return;
        }
    }

    xset_seq(&m->_locked, 0);
    for (int i = 0, usec = 1; i < SPINS; i++) {
        if (xget_seq(&m->_locked) == 1) {
            return;
        }
        usec = ftx_backoff(usec);
    }

    xset_seq(&m->_contended, 0);
    ftx_wake(&m->_state);
}
