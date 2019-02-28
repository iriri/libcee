#ifndef CEE_EVT_H
#define CEE_EVT_H
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <cee/ftx.h>

typedef struct evt {
    union {
        /* TODO: Make 64 bit ftx tags work too */
        _Atomic uint64_t _u64;
        struct {
            _Atomic ftx _state, _waiterc;
        };
    };
} evt;

evt evt_make(void);
void evt_wait(evt *);
bool evt_trywait(evt *);
bool evt_timedwait(evt *, struct timespec);
void evt_post(evt *);
#endif
