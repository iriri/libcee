#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <cee/ftx.h>
#include <cee/xops.h>

#include <cee/evt.h>

void
evt_wait(evt *e) {
    ftx state;
    while ((state = xchg_seq(&e->_state, 0)) != 2) {
        if (state == 1 || xcas_s_seq_rlx(&e->_state, &state, 1)) {
            ftx_wait(&e->_state, 1);
        }
    }
}

bool
evt_trywait(evt *e) {
    ftx exp = 2;
    return xcas_s_seq_rlx(&e->_state, &exp, 0);
}

bool
evt_timedwait(evt *e, struct timespec *timeout) {
    ftx state;
    while ((state = xchg_seq(&e->_state, 0)) != 2) {
        if (state == 1 || xcas_s_seq_rlx(&e->_state, &state, 1)) {
            if (ftx_timedwait(&e->_state, 1, timeout) == ETIMEDOUT) {
                return xchg_seq(&e->_state, 0) == 2;
            }
        }
    }
    return true;
}

void
evt_post(evt *e) {
    if (xchg_seq(&e->_state, 2) == 1) {
        ftx_wake(&e->_state);
    }
}
