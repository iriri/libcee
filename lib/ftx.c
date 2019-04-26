#include <stdlib.h>
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

int
ftx_backoff(int usec) {
    usleep(usec);
    return usec > 1 << 11 ? usec - (rand() % usec) : usec << 1;
}
