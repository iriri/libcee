#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <cee/ftx.h>
#include <cee/xops.h>

#include <cee/evt.h>

static const ftx CONSUMED = 0;
static const ftx WAITING = 1;
static const ftx SIGNALLED = 2;

void
evt_wait(evt *e) {
    ftx state;
    while ((state = xchg_seq(&e->_state, CONSUMED)) != SIGNALLED) {
        if (state == WAITING || xcas_s_seq_rlx(&e->_state, &state, WAITING)) {
            ftx_wait(&e->_state, WAITING);
        }
    }
}

bool
evt_trywait(evt *e) {
    ftx signalled = SIGNALLED;
    return xcas_s_seq_rlx(&e->_state, &signalled, CONSUMED);
}

bool
evt_timedwait(evt *e, const struct timespec *timeout) {
    ftx state;
    while ((state = xchg_seq(&e->_state, CONSUMED)) != SIGNALLED) {
        if (state == WAITING || xcas_s_seq_rlx(&e->_state, &state, WAITING)) {
            if (ftx_timedwait(&e->_state, WAITING, timeout) == ETIMEDOUT) {
                return xchg_seq(&e->_state, CONSUMED) == SIGNALLED;
            }
        }
    }
    return true;
}

void
evt_post(evt *e) {
    if (xchg_seq(&e->_state, SIGNALLED) == WAITING) {
        ftx_wake(&e->_state);
    }
}
