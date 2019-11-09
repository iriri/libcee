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

useconds_t
ftx_backoff(useconds_t usec) {
    usleep(usec);
    return usec > 1 << 7 ? usec : usec << 1;
}
