#ifndef CEE_EVT_H
#define CEE_EVT_H
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <cee/ftx.h>

typedef struct evt {
    _Atomic ftx _state;
} evt;

#define evt_make() (const evt){0}

void evt_wait(evt *);
bool evt_trywait(evt *);
bool evt_timedwait(evt *, const struct timespec *);
void evt_post(evt *);
#endif
