#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <cee/ftx.h>
#include <cee/xops.h>

#include <cee/evt.h>

typedef union evt_un64_ {
    uint64_t u64;
    struct {
        ftx state, waiterc;
    };
} evt_un64_;

evt
evt_make(void) {
    return (evt){0};
}

void
evt_wait(evt *e) {
    xadd_acr(&e->_waiterc, 1);
    while (xchg_seq(&e->_state, 0) == 0) {
        ftx_wait(&e->_state, 0);
    }
    xsub_acr(&e->_waiterc, 1);
}

bool
evt_trywait(evt *e) {
    return xchg_seq(&e->_state, 0) == 1;
}

bool
evt_timedwait(evt *e, struct timespec timeout) {
    int rc = 0;
    xadd_acr(&e->_waiterc, 1);
    while (rc != ETIMEDOUT && xchg_seq(&e->_state, 0) == 0) {
        rc = ftx_timedwait(&e->_state, 0, timeout);
    }
    xsub_acr(&e->_waiterc, 1);
    return rc != ETIMEDOUT;
}

void
evt_post(evt *e) {
    evt_un64_ u = {xget_acq(&e->_u64)};
    while (!xcas_w_seq_acq(&e->_u64, &u.u64, u.u64 + 1)) {
        if (u.state == 1) {
            return;
        }
    }
    if (u.waiterc != 0) {
        ftx_wake(&e->_state);
    }
}
