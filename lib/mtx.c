#if MTX_IMPL == 0
/* I didn't write this (completely) out of NIH symdrome. I just wanted a mutex
 * that didn't take up 5/8ths of a cache line, unlike glibc's. This
 * implementation was originally shamelessly copied from "Mutexes and Condition
 * Variables using Futexes" by Steven Fuerst but has evolved somewhat since,
 * probably for the worse. */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <cee/cee.h>
#include <cee/ftx.h>
#include <cee/xops.h>

#include <cee/mtx.h>

static const int SPINS = 64;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
static const ftx LOCKED = 0x00000001;
static const ftx WAITER = 0x00000100;
#else
static const ftx LOCKED = 0x01000000;
static const ftx WAITER = 0x00000001;
#endif

void
mtx_lock_(mtx *m) {
    for (int i = 0; i < SPINS; i++) {
        if (xget_rlx(&m->_locked) == 0) {
            uint8_t unlocked = 0;
            if (xcas_w_acr_rlx(&m->_locked, &unlocked, 1)) {
                return;
            }
        }
        cee_pause8();
    }

    for (ftx s = xadd_acr(&m->_state, WAITER); ; s = xget_rlx(&m->_state)) {
        if ((s & LOCKED) == 0x0) {
            uint8_t unlocked = 0;
            if (xcas_w_acr_rlx(&m->_locked, &unlocked, 1)) {
                xsub_acr(&m->_state, WAITER);
                return;
            }
        }

        ftx_wait(&m->_state, s);
    }
}

bool
mtx_trylock_(mtx *m) {
    uint8_t unlocked = 0;
    return xcas_s_acr_rlx(&m->_locked, &unlocked, 1);
}

bool
mtx_timedlock_(mtx *m, const struct timespec *timeout) {
    for (int i = 0; i < SPINS; i++) {
        if (xget_rlx(&m->_locked) == 0) {
            uint8_t unlocked = 0;
            if (xcas_w_acr_rlx(&m->_locked, &unlocked, 1)) {
                return true;
            }
        }
        cee_pause8();
    }

    for (ftx s = xadd_acr(&m->_state, WAITER); ; s = xget_rlx(&m->_state)) {
        if ((s & LOCKED) == 0x0) {
            uint8_t unlocked = 0;
            if (xcas_w_acr_rlx(&m->_locked, &unlocked, 1)) {
                xsub_acr(&m->_state, WAITER);
                return true;
            }
        }

        if (ftx_timedwait(&m->_state, s, timeout) == ETIMEDOUT) {
            xsub_acr(&m->_state, WAITER);
            return false;
        }
    }
}

void
mtx_unlock_(mtx *m) {
    ftx prev = xsub_acr(&m->_state, LOCKED);
    if (prev == LOCKED) {
        return;
    }

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (prev >> 8 > 4) {
#else
    if ((prev ^ LOCKED) > 4) {
#endif
        for (int i = 0; i < SPINS; i++) {
            cee_pause8();
            if (xget_rlx(&m->_locked) == 1) {
                return;
            }
        }
    }
    ftx_wake(&m->_state);
}
#else
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <cee/cee.h>
#include <cee/ftx.h>
#include <cee/xops.h>

#include <cee/mtx.h>

static const int SPINS = 64;
static const ftx LOCKBIT = 0x80000000;
static const ftx LOCKED = LOCKBIT | 0x1;

void
mtx_lock_(mtx *m) {
    if (xget_rlx(&m->_state) == 0x0) {
        ftx unlocked = 0x0;
        if (xcas_w_acr_rlx(&m->_state, &unlocked, LOCKED)) {
            return;
        }
    }

    int i = 0;
    for (ftx s = xadd_rlx(&m->_state, 0x1); ; ) {
        if ((s & LOCKBIT) != 0x0) {
            if (i < SPINS) {
                i++;
                cee_pause8();
            } else {
                ftx_wait(&m->_state, s);
            }
            s = xget_rlx(&m->_state);
        } else if (xcas_w_acr_rlx(&m->_state, &s, s | LOCKBIT)) {
            return;
        }
    }
}

bool
mtx_trylock_(mtx *m) {
    ftx unlocked = 0;
    return xcas_s_acr_rlx(&m->_state, &unlocked, LOCKED);
}

bool
mtx_timedlock_(mtx *m, const struct timespec *timeout) {
    if (xget_rlx(&m->_state) == 0x0) {
        ftx unlocked = 0x0;
        if (xcas_w_acr_rlx(&m->_state, &unlocked, LOCKED)) {
            return true;
        }
    }

    int i = 0;
    for (ftx s = xadd_rlx(&m->_state, 0x1); ; ) {
        if ((s & LOCKBIT) != 0x0) {
            if (i < SPINS) {
                i++;
                cee_pause8();
            } else if (ftx_timedwait(&m->_state, s, timeout) == ETIMEDOUT) {
                return false;
            }
            s = xget_rlx(&m->_state);
        } else if (xcas_w_acr_rlx(&m->_state, &s, s | LOCKBIT)) {
            return true;
        }
    }
}

void
mtx_unlock_(mtx *m) {
    ftx prev = xsub_acr(&m->_state, LOCKED);
    if (prev == LOCKED) {
        return;
    }

    if ((prev ^ LOCKBIT) > 4) {
        for (int i = 0; i < SPINS; i++) {
            cee_pause8();
            if ((xget_rlx(&m->_state) & LOCKBIT) != 0x0) {
                return;
            }
        }
    }
    ftx_wake(&m->_state);
}
#endif
