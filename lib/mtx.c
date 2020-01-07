/* I didn't write this (completely) out of NIH symdrome. I just wanted a mutex
 * that didn't take up 5/8ths of a cache line, unlike glibc's. This
 * implementation was originally shamelessly copied from "Mutexes and Condition
 * Variables using Futexes" by Steven Fuerst but has evolved somewhat since,
 * probably for the worse. The spinning strategy of mostly yielding rather than
 * pausing is inspired by Webkit's Web Template Framework. */
#include <errno.h>
#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <cee/cee.h>
#include <cee/ftx.h>
#include <cee/xops.h>

#include <cee/mtx.h>

static const int NPAUSES = 8;
static const int NYIELDS = 32;
static const int NMORPHS = 128;
/* `NCONTENDED = 0` actually performs best in synthetic microbenchmarks but I
 * really hate the idea of unnecessarily spinning in `mtx_unlock`. */
static const ftx NCONTENDED = 4;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
static const ftx LOCKED = 0x00000001;
static const ftx WAITER = 0x00000100;
#else
static const ftx LOCKED = 0x01000000;
static const ftx WAITER = 0x00000001;
#endif

bool
mtx_trylock_(mtx *m) {
    uint8_t unlocked = 0;
    return xcas_s_acr_rlx(&m->_locked, &unlocked, 1);
}

static bool
mtx_lock__(mtx *m, const struct timespec *timeout) {
    for (int i = 0; i < NPAUSES + NYIELDS; i++) {
        if (xget_rlx(&m->_locked) == 0) {
            uint8_t unlocked = 0;
            if (xcas_w_acr_rlx(&m->_locked, &unlocked, 1)) {
                return true;
            }
        }

        if (i < NPAUSES) {
            cee_pause8();
        } else {
            sched_yield();
        }
    }

    for (ftx s = xadd_rlx(&m->_state, WAITER); ; s = xget_rlx(&m->_state)) {
        if ((s & LOCKED) == 0x0) {
            uint8_t unlocked = 0;
            if (xcas_w_acr_rlx(&m->_locked, &unlocked, 1)) {
                xsub_rlx(&m->_state, WAITER);
                return true;
            }
        }

        if (ftx_timedwait(&m->_state, s, timeout) == ETIMEDOUT) {
            xsub_rlx(&m->_state, WAITER);
            return false;
        }
    }
}

void
mtx_lock_(mtx *m) {
    if (!mtx_trylock_(m)) {
        mtx_lock__(m, NULL);
    }
}

bool mtx_timedlock_(mtx *m, const struct timespec *timeout) {
    return mtx_trylock_(m) ? true : mtx_lock__(m, timeout);
}

static void
mtx_unlock__(mtx *m, ftx prev) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (prev >> 8 > NCONTENDED) {
#else
    if ((prev ^ LOCKED) > NCONTENDED) {
#endif
        for (int i = 0; i < NMORPHS; i++) {
            cee_pause();
            if (xget_rlx(&m->_locked) == 1) {
                return;
            }
        }
    }
    ftx_wake(&m->_state);
}

void
mtx_unlock_(mtx *m) {
    ftx prev = xsub_acr(&m->_state, LOCKED);
    if (prev != LOCKED) {
        mtx_unlock__(m, prev);
    }
}
