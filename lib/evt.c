#include <errno.h>
#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <cee/cee.h>
#include <cee/ftx.h>
#include <cee/xops.h>

#include <cee/evt.h>

static const int NPAUSES = 8;
static const int NYIELDS = 8;
static const ftx CONSUMED = 0;
static const ftx WAITING = 1;
static const ftx SIGNALED = 2;

bool
evt_trywait(evt *e) {
    ftx signalled = SIGNALED;
    return xcas_s_acr_rlx(&e->_state, &signalled, CONSUMED);
}

static bool
evt_wait_(evt *e, const struct timespec *timeout) {
    for (int i = 0; ; i++) {
        if (xget_rlx(&e->_state) == SIGNALED) {
            if (xchg_acr(&e->_state, CONSUMED) == SIGNALED) {
                return true;
            }
        }

        if (i < NPAUSES) {
            cee_pause8();
        } else if (i < NPAUSES + NYIELDS) {
            sched_yield();
        } else {
            break;
        }
    }

    while (xchg_acr(&e->_state, CONSUMED) != SIGNALED) {
        ftx state = CONSUMED;
        if (
            xcas_s_acr_rlx(&e->_state, &state, WAITING) &&
            ftx_timedwait(&e->_state, WAITING, timeout) == ETIMEDOUT
        ) {
            return false;
        }
    }
    return true;
}

void
evt_wait(evt *e) {
    if (!evt_trywait(e)) {
        evt_wait_(e, NULL);
    }
}

bool
evt_timedwait(evt *e, const struct timespec *timeout) {
    return evt_trywait(e) ? true : evt_wait_(e, timeout);
}

void
evt_post(evt *e) {
    if (xchg_acr(&e->_state, SIGNALED) == WAITING) {
        ftx_wake(&e->_state);
    }
}
