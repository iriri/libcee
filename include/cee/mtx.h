#ifndef CEE_EVT_H
#define CEE_EVT_H
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <cee/ftx.h>

/* I didn't write this (completely) out of NIH symdrome. I just wanted a mutex
 * that didn't take up 5/8ths of a cache line, unlike glibc's. As we can see
 * here, this mutex is nothing more than a single `ftx`.
 *
 * Ironically the bloatedness of glibc's mutex is sometimes a feature.
 * Sometimes it just happens to push the right fields into different cache
 * lines, improving performance by preventing false sharing. */
typedef struct mtx {
    union {
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
    };
} mtx;

/* Avoid name conflicts with C11 threads */
#define mtx_lock(m) mtx_lock_(m)
#define mtx_trylock(m) mtx_trylock_(m)
#define mtx_timedlock(m, timeout) mtx_timedlock_(m, timeout)
#define mtx_unlock(m) mtx_unlock_(m)

mtx mtx_make(void);
void mtx_lock_(mtx *);
bool mtx_trylock_(mtx *);
bool mtx_timedlock_(mtx *, struct timespec);
void mtx_unlock_(mtx *);
#endif
