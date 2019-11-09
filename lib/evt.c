#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <cee/ftx.h>
#include <cee/xops.h>

#include <cee/evt.h>

static const int SPINS = 64;
static const ftx CONSUMED = 0;
static const ftx WAITING = 1;
static const ftx SIGNALED = 2;

void
evt_wait(evt *e) {
    useconds_t usec = 1;
    for (int i = 0; i < SPINS; i++) {
        if (xchg_acr(&e->_state, CONSUMED) == SIGNALED) {
            return;
        }
        usec = ftx_backoff(usec);
    }

    while (xchg_acr(&e->_state, CONSUMED) != SIGNALED) {
        ftx state = CONSUMED;
        if (xcas_s_acr_rlx(&e->_state, &state, WAITING)) {
            ftx_wait(&e->_state, WAITING);
        }
    }
}

bool
evt_trywait(evt *e) {
    ftx signalled = SIGNALED;
    return xcas_s_acr_rlx(&e->_state, &signalled, CONSUMED);
}

bool
evt_timedwait(evt *e, const struct timespec *timeout) {
    useconds_t usec = 1;
    for (int i = 0; i < SPINS; i++) {
        if (xchg_acr(&e->_state, CONSUMED) == SIGNALED) {
            return true;
        }
        usec = ftx_backoff(usec);
    }

    while (xchg_acr(&e->_state, CONSUMED) != SIGNALED) {
        ftx state = CONSUMED;
        if (
            xcas_s_acr_rlx(&e->_state, &state, WAITING) &&
            ftx_timedwait(&e->_state, WAITING, timeout) == ETIMEDOUT
        ) {
            return xchg_acr(&e->_state, CONSUMED) == SIGNALED;
        }
    }
    return true;
}

void
evt_post(evt *e) {
    if (xchg_acr(&e->_state, SIGNALED) == WAITING) {
        ftx_wake(&e->_state);
    }
}
