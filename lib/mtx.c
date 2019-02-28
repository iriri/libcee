/* Shamelessly copied from "Mutexes and Condition Variables using Futexes" by
 * Steven Fuerst. */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <cee/ftx.h>
#include <cee/xops.h>

#include <cee/mtx.h>

#define SPINS 256

const uint32_t MTX_LOCKED_CONTEND_ = 0x0101;

mtx
mtx_make(void) {
    return (mtx){0};
}

void
mtx_lock_(mtx *m) {
    uint8_t locked;
    for (int i = 0; i < SPINS; i++) {
        locked = 0;
        if (xcas_s_seq_acq(&m->_locked, &locked, true)) {
            return;
        }
        usleep(i * i); // TODO: smarter backoff
    }

    while (xchg_seq(&m->_state, MTX_LOCKED_CONTEND_)) {
        ftx_wait(&m->_state, MTX_LOCKED_CONTEND_);
    }
}

bool
mtx_trylock_(mtx *m) {
    uint8_t exp = 0;
    return xcas_s_seq_rlx(&m->_locked, &exp, true);
}

bool
mtx_timedlock_(mtx *m, struct timespec timeout) {
    uint8_t locked;
    for (int i = 0; i < SPINS; i++) {
        locked = 0;
        if (xcas_s_seq_acq(&m->_locked, &locked, true)) {
            return true;
        }
        usleep(i * i);
    }

    int rc = 0;
    while (rc != ETIMEDOUT && xchg_seq(&m->_state, MTX_LOCKED_CONTEND_)) {
        rc = ftx_timedwait(&m->_state, MTX_LOCKED_CONTEND_, timeout);
    }
    return rc != ETIMEDOUT;
}

void
mtx_unlock_(mtx *m) {
    ftx exp = 1;
    if (xget_rlx(&m->_state) == 1 && xcas_s_seq_rlx(&m->_state, &exp, 0)) {
        return;
    }
    xset_seq(&m->_locked, 0);
    for (int i = 0; i < SPINS; i++) {
        if (xget_seq(&m->_locked) == 1) {
            return;
        }
        usleep(i * i);
    }

    xset_seq(&m->_contend, 0);
    ftx_wake(&m->_state);
}
