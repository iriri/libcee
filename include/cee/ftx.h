#ifndef CEE_FTX_H
#define CEE_FTX_H
#include <stdint.h>
#include <time.h>
#include <unistd.h>

typedef uint32_t ftx;

int ftx_wait(_Atomic ftx *, ftx);
int ftx_timedwait(_Atomic ftx *, ftx, const struct timespec *);
void ftx_wake(_Atomic ftx *);
void ftx_wakeall(_Atomic ftx *);
#endif
