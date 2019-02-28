#ifndef CEE_FTX_H
#define CEE_FTX_H
#include <stdint.h>
#include <time.h>

#ifdef __linux__
typedef int ftx;
#elif __FreeBSD__
#error "soon"
#elif __OpenBSD__
#error "soon"
#elif __APPLE__
#error "soon"
#else
#error "probably never"
#endif

int ftx_wait(_Atomic ftx *, ftx);
int ftx_timedwait(_Atomic ftx *, ftx, struct timespec);
void ftx_wake(_Atomic ftx *);
void ftx_wakeall(_Atomic ftx *);
struct timespec ftx_rel_to_abs(uint64_t);
#endif
