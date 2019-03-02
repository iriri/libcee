#ifndef CEE_MTX_H
#define CEE_MTX_H
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <cee/ftx.h>

typedef union mtx {
    _Atomic ftx _state;
    struct {
        /* TODO: Make 64 bit ftx tags work too */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        _Atomic uint8_t _locked, _contend;
        const uint8_t _pad, _pad1;
#else
        const uint8_t _pad1, _pad;
        _Atomic uint8_t _contend, _locked;
#endif
    };
} mtx;

#define mtx_make() (mtx){0}

/* Avoid name conflicts with C11 threads */
#define mtx_lock(m) mtx_lock_(m)
#define mtx_trylock(m) mtx_trylock_(m)
#define mtx_timedlock(m, timeout) mtx_timedlock_(m, timeout)
#define mtx_unlock(m) mtx_unlock_(m)

void mtx_lock_(mtx *);
bool mtx_trylock_(mtx *);
bool mtx_timedlock_(mtx *, struct timespec *);
void mtx_unlock_(mtx *);
#endif
