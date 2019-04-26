#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <time.h>

#include <cee/cee.h>

#include <cee/ftx.h>

/* From the darwin-xnu sources */
static const uintptr_t UL_COMPARE_AND_WAIT = 0x00000001;
static const uintptr_t ULF_WAKE_ALL = 0x00000100;

int
ftx_wait(_Atomic ftx *f, ftx val) {
    /* I hate errno so much */
    while (syscall(SYS_ulock_wait, UL_COMPARE_AND_WAIT, f, val, 0) != 0) {
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
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (timeout->tv_sec < start.tv_sec) {
        return ETIMEDOUT;
    }
    time_t sec = timeout->tv_sec - start.tv_sec;
    long nsec = timeout->tv_nsec - start.tv_nsec;
    if (nsec < 0) {
        if (sec-- == 0) {
            return ETIMEDOUT;
        }
        nsec += 1000000000;
    }
    cee_assert(sec <= UINT32_MAX / 1000000);
    uint32_t sec1 = sec * 1000000;
    uint32_t usec = sec1 + (nsec / 1000);
    cee_assert(usec >= sec1);

    while (syscall(SYS_ulock_wait, UL_COMPARE_AND_WAIT, f, val, usec) != 0) {
        switch (errno) {
        case EINTR: {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            uint32_t usec1 = usec - ((now.tv_sec - start.tv_sec) * 1000000);
            long nsec = now.tv_nsec - start.tv_nsec;
            if (nsec >= 0) {
                usec1 -= nsec / 1000;
            } else {
                usec1 -= (1000000000 + nsec) / 1000;
            }
            if (usec1 > usec) {
                return ETIMEDOUT;
            }
            usec = usec1;
            break;
        }
        case EAGAIN:
        case ETIMEDOUT: return errno;
        default: cee_assert(false);
        }
    }
    return 0;
}

void
ftx_wake(_Atomic ftx *f) {
    syscall(SYS_ulock_wake, UL_COMPARE_AND_WAIT, f, 0);
}

void
ftx_wakeall(_Atomic ftx *f) {
    syscall(SYS_ulock_wake, UL_COMPARE_AND_WAIT | ULF_WAKE_ALL, f, 0);
}
#pragma clang diagnostic pop
#pragma GCC diagnostic pop
