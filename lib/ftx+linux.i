#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <stdbool.h>
#include <sys/syscall.h>
#include <time.h>

#include <cee/cee.h>

#include <cee/ftx.h>


int
ftx_wait(_Atomic ftx *f, ftx val) {
    /* I hate errno so much */
    while (
        syscall(SYS_futex, f, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0) != 0
    ) {
        switch (errno) {
        case EINTR: break;
        case EAGAIN: return errno;
        default: cee_assert(false);
        }
    }
    return 0;
}

int
ftx_timedwait(_Atomic ftx *f, ftx val, const struct timespec *timeout) {
    while (syscall(
        SYS_futex,
        f,
        FUTEX_WAIT_BITSET_PRIVATE,
        val,
        timeout,
        NULL,
        FUTEX_BITSET_MATCH_ANY) != 0
    ) {
        switch (errno) {
        case EINTR: break;
        case EAGAIN:
        case ETIMEDOUT: return errno;
        default: cee_assert(false);
        }
    }
    return 0;
}

void
ftx_wake(_Atomic ftx *f) {
    syscall(SYS_futex, f, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}

void
ftx_wakeall(_Atomic ftx *f) {
    syscall(SYS_futex, f, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
}
