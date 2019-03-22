#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include "ftx+linux.i"
#elif __FreeBSD__
#error "soon"
#elif __OpenBSD__
#error "soon"
#elif __APPLE__
#include "ftx+apple.i"
#else
#error "probably never"
#endif

struct timespec
ftx_rel_to_abs(uint64_t timeout) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += (timeout % 1000000) * 1000;
    ts.tv_sec += (ts.tv_nsec / 1000000000) + (timeout / 1000000);
    ts.tv_nsec %= 1000000000;
    return ts;
}

int
ftx_backoff(int usec) {
    usleep(usec);
    return usec > 1 << 11 ? usec - (rand() % usec) : usec << 1;
}
