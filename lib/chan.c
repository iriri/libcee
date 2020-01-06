/* The semantics of this library are inspired by channels from the Go
 * programming language.
 *
 * The implementations of sending, receiving, and selection are based on
 * algorithms described in "Go channels on steroids" by Dmitry Vyukov. */
#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cee/cee.h>
#include <cee/evt.h>
#include <cee/mtx.h>
#include <cee/xops.h>

#include <cee/chan.h>

static const int NTRIES = 4;
static const size_t ALT_NIL = CHAN_WBLOCK;
static const size_t ALT_MAGIC = CHAN_CLOSED;

void *
chan_make_(size_t msgsz, size_t cap, size_t cellsz) {
    chan_ *c;
    if (cap == 0) {
        cee_assert(msgsz <= UINT32_MAX && (c = calloc(1, sizeof(c->unbuf))));
        c->unbuf.msgsz = msgsz;
    } else {
        cee_assert(cap <= UINT32_MAX && cap <= SIZE_MAX / msgsz);
        cee_assert((c = calloc(
            1, offsetof(chan_buf_(cee_u256_), buf) + (cap * cellsz))));
        c->buf_cee_u256_.cap = cap;
        xset_rlx(&c->buf_cee_u256_.read.lap, 1);
    }
    xset_rlx(&c->hdr.openc, 1);
    xset_rlx(&c->hdr.refc, 1);
    xset_rlx(&c->hdr.sendq.next, (chan_waiter_ *)&c->hdr.sendq);
    xset_rlx(&c->hdr.sendq.prev, (chan_waiter_ *)&c->hdr.sendq);
    xset_rlx(&c->hdr.recvq.next, (chan_waiter_ *)&c->hdr.recvq);
    xset_rlx(&c->hdr.recvq.prev, (chan_waiter_ *)&c->hdr.recvq);
    return c;
}

void *
chan_drop_(chan_ *c) {
    switch (xsub_acr(&c->hdr.refc, 1)) {
    case 0: cee_assert(false);
    case 1:
        mtx_lock(&c->hdr.lock);
        free(c); // fallthrough
    default: return NULL;
    }
}

void *
chan_ptrdrop_(chan_ *c) {
    switch (xsub_acr(&c->hdr.refc, 1)) {
    case 0: cee_assert(false);
    case 1:
        if (c->hdr.cap > 0) {
            chan_un64_ read = {xget_rlx(&c->buf_cee_u64_.read.u64)};
            while (read.lap == xget_rlx(&c->buf_cee_u64_.buf[read.idx].lap)) {
                free((void *)c->buf_cee_u64_.buf[read.idx].msg);
                read.u64 = read.idx + 1 < c->buf_cee_u64_.cap ?
                    read.u64 + 1 : (uint64_t)(read.lap + 2) << 32;
            }
        }
        mtx_lock(&c->hdr.lock);
        free(c); // fallthrough
    default: return NULL;
    }
}

#define CHAN_FNDROP_DECL(T, _a) \
    void * \
    buf_fndrop_##T(chan_buf_(T) *c, void (*fn)(void *)) { \
        switch (xsub_acr(&c->refc, 1)) { \
        case 0: cee_assert(false); \
        case 1: { \
            chan_un64_ read = {xget_rlx(&c->read.u64)}; \
            while (read.lap == xget_rlx(&c->buf[read.idx].lap)) { \
                fn(&c->buf[read.idx].msg); \
                read.u64 = read.idx + 1 < c->cap ? \
                    read.u64 + 1 : (uint64_t)(read.lap + 2) << 32; \
            } \
            mtx_lock(&c->lock); \
            free(c); \
        } /* fallthrough */ \
        default: return NULL; \
        } \
    } \
 \
    void * \
    chan_fndrop_##T##_(chan_ *c, void (*fn)(void *)) { \
        return c->hdr.cap > 0 ? \
            buf_fndrop_##T(&c->buf_##T, fn) : chan_drop_(c); \
    }
CHAN_DEF_ALL_(CHAN_FNDROP_DECL, )

static void
waitq_push(chan_waiter_root_ *waitq, chan_waiter_ *w) {
    w->hdr.next = (chan_waiter_ *)waitq;
    w->hdr.prev = xget_rlx(&waitq->prev);
    xset_seq(&xget_rlx(&waitq->prev)->root.next, w);
    xset_rlx(&waitq->prev, w);
}

static chan_waiter_ *
waitq_shift(chan_waiter_root_ *waitq) {
    chan_waiter_ *w = xget_rlx(&waitq->next);
    if (&w->root == waitq) {
        return NULL;
    }
    xset_seq(&w->hdr.prev->root.next, w->hdr.next);
    xset_rlx(&w->hdr.next->root.prev, w->hdr.prev);
    w->hdr.prev = NULL; // for waitq_remove
    xset_rlx(&w->hdr.ref, true);
    return w;
}

static bool
waitq_remove(chan_waiter_ *w) {
    if (!w->hdr.prev) { // already shifted off the front
        return false;
    }
    xset_seq(&w->hdr.prev->root.next, w->hdr.next);
    xset_rlx(&w->hdr.next->root.prev, w->hdr.prev);
    return true;
}

void *
chan_close_(chan_ *c) {
    switch (xsub_acr(&c->hdr.openc, 1)) {
    case 0: cee_assert(false);
    case 1:
        mtx_lock(&c->hdr.lock);
        if (c->hdr.cap > 0) {
            chan_waiter_buf_ *w;
            /* UBSan complains about union member accesses when the pointer is
             * `NULL` but I don't think I care since we aren't actually
             * dereferencing the pointer. */
            while ((w = &waitq_shift(&c->hdr.sendq)->buf)) {
                evt_post(w->trg);
            }
            while ((w = &waitq_shift(&c->hdr.recvq)->buf)) {
                evt_post(w->trg);
            }
        } else {
            chan_waiter_unbuf_ *w;
            while ((w = &waitq_shift(&c->unbuf.sendq)->unbuf)) {
                w->closed = true;
                evt_post(w->trg);
            }
            while ((w = &waitq_shift(&c->unbuf.recvq)->unbuf)) {
                w->closed = true;
                evt_post(w->trg);
            }
        }
        mtx_unlock(&c->hdr.lock); // fallthrough
    default: return NULL;
    }
}

static void
buf_waitq_shift(chan_waiter_root_ *waitq, mtx *lock) {
    while (&xget_seq(&waitq->next)->root != waitq) {
        mtx_lock(lock);
        chan_waiter_buf_ *w = &waitq_shift(waitq)->buf;
        mtx_unlock(lock);
        if (w) {
            if (w->alt_state) {
                size_t magic = ALT_MAGIC;
                if (!xcas_s_rlx_rlx(w->alt_state, &magic, w->alt_id)) {
                    xset_rel(&w->ref, false);
                    continue;
                }
            }
            evt_post(w->trg);
        }
        return;
    }
}

#define BUF_TRYOP_DECL(T, _a) \
    static chan_rc \
    buf_trysend##T(chan_buf_(T) *c, T msg) { \
        if (xget_acq(&c->openc) == 0) { \
            return CHAN_CLOSED; \
        } \
 \
        chan_un64_ write = {xget_rlx(&c->write.u64)}; \
        for (int i = 0; ; ) {\
            chan_cell_(T) *cell = c->buf + write.idx; \
            uint32_t lap = xget_acq(&cell->lap); \
            if (write.lap == lap) { \
                uint64_t write1 = write.idx + 1 < c->cap ? \
                    write.u64 + 1 : (uint64_t)(write.lap + 2) << 32; \
                if (!xcas_w_rlx_rlx(&c->write.u64, &write.u64, write1)) { \
                    continue; \
                } \
                cell->msg = msg; \
                xset_rel(&cell->lap, lap + 1); \
                buf_waitq_shift(&c->recvq, &c->lock); \
                return CHAN_OK; \
            } \
\
            if ((int32_t)(write.lap - lap) > 0) { \
                if (++i > NTRIES) { \
                    return CHAN_WBLOCK; \
                } \
                sched_yield(); \
            } \
            if (xget_acq(&c->openc) == 0) { \
                return CHAN_CLOSED; \
            } \
            write.u64 = xget_rlx(&c->write.u64); \
        } \
    } \
 \
    static chan_rc \
    buf_tryrecv##T(chan_buf_(T) *c, T *msg) { \
        chan_un64_ read = {xget_rlx(&c->read.u64)}; \
        for (int i = 0; ; ) { \
            chan_cell_(T) *cell = c->buf + read.idx; \
            uint32_t lap = xget_acq(&cell->lap); \
            if (read.lap == lap) { \
                uint64_t read1 = read.idx + 1 < c->cap ? \
                    read.u64 + 1 : (uint64_t)(read.lap + 2) << 32; \
                if (!xcas_w_rlx_rlx(&c->read.u64, &read.u64, read1)) { \
                    continue; \
                } \
                *msg = cell->msg; \
                xset_rel(&cell->lap, lap + 1); \
                buf_waitq_shift(&c->sendq, &c->lock); \
                return CHAN_OK; \
            } \
 \
            if ((int32_t)(read.lap - lap) > 0) { \
                if (xget_acq(&c->openc) == 0) { \
                    return CHAN_CLOSED; \
                } \
                if (++i > NTRIES) { \
                    return CHAN_WBLOCK; \
                } \
                sched_yield(); \
            } \
            read.u64 = xget_rlx(&c->read.u64); \
        } \
    }
CHAN_DEF_ALL_(BUF_TRYOP_DECL, )

static chan_rc
unbuf_try(chan_unbuf_ *c, void *msg, chan_waiter_root_ *waitq) {
    while (xget_acq(&c->openc) > 0) {
        if (&xget_rlx(&waitq->next)->root == waitq) {
            return CHAN_WBLOCK;
        }

        mtx_lock(&c->lock);
        if (xget_acq(&c->openc) == 0) {
            mtx_unlock(&c->lock);
            return CHAN_CLOSED;
        }
        chan_waiter_unbuf_ *w = &waitq_shift(waitq)->unbuf;
        mtx_unlock(&c->lock);
        if (!w) {
            return CHAN_WBLOCK;
        }
        if (w->alt_state) {
            size_t magic = ALT_MAGIC;
            if (!xcas_s_rlx_rlx(w->alt_state, &magic, w->alt_id)) {
                xset_rel(&w->ref, false);
                continue;
            }
        }

        if (waitq == &c->recvq) {
            memcpy(w->msg, msg, c->msgsz);
        } else {
            memcpy(msg, w->msg, c->msgsz);
        }
        evt_post(w->trg);
        return CHAN_OK;
    }
    return CHAN_CLOSED;
}

#define BUF_OP_DECL(T, _a) \
    static chan_rc \
    buf_send_##T(chan_buf_(T) *c, T msg, const struct timespec *timeout) { \
        evt trg = evt_make(); \
        chan_waiter_buf_ w = {.trg = &trg, .alt_id = ALT_NIL}; \
        for ( ; ; ) { \
            chan_rc rc = buf_trysend##T(c, msg); \
            if (rc != CHAN_WBLOCK) { \
                return rc; \
            } \
 \
            mtx_lock(&c->lock); \
            if (xget_acq(&c->openc) == 0) { \
                mtx_unlock(&c->lock); \
                return CHAN_CLOSED; \
            } \
            /* TODO: Casts are evil. Figure out how to get rid of these. */ \
            waitq_push(&c->sendq, (chan_waiter_ *)&w); \
            chan_un64_ write = {xget_rlx(&c->write.u64)}; \
            if ( \
                (int32_t)(write.lap - xget_rlx(&c->buf[write.idx].lap)) <= 0 \
            ) { \
                waitq_remove((chan_waiter_ *)&w); \
                mtx_unlock(&c->lock); \
                continue; \
            } \
            mtx_unlock(&c->lock); \
 \
            if (timeout == NULL) { \
                evt_wait(&trg); \
            } else if (!evt_timedwait(&trg, timeout)) { \
                mtx_lock(&c->lock); \
                bool onqueue = waitq_remove((chan_waiter_ *)&w); \
                mtx_unlock(&c->lock); \
                if (!onqueue) { \
                    evt_wait(w.trg); \
                    chan_rc rc = buf_trysend##T(c, msg); \
                    if (rc != CHAN_WBLOCK) { \
                        return rc; \
                    } \
                } \
                return CHAN_WBLOCK; \
            } \
        } \
    } \
 \
    static chan_rc \
    buf_recv_##T(chan_buf_(T) *c, T *msg, const struct timespec *timeout) { \
        evt trg = evt_make(); \
        chan_waiter_buf_ w = {.trg = &trg, .alt_id = ALT_NIL}; \
        for ( ; ; ) { \
            chan_rc rc = buf_tryrecv##T(c, msg); \
            if (rc != CHAN_WBLOCK) { \
                return rc; \
            } \
 \
            mtx_lock(&c->lock); \
            waitq_push(&c->recvq, (chan_waiter_ *)&w); \
            chan_un64_ read = {xget_rlx(&c->read.u64)}; \
            if ((int32_t)(read.lap - xget_rlx(&c->buf[read.idx].lap)) <= 0) { \
                waitq_remove((chan_waiter_ *)&w); \
                mtx_unlock(&c->lock); \
                continue; \
            } \
            if (xget_acq(&c->openc) == 0) { \
                waitq_remove((chan_waiter_ *)&w); \
                mtx_unlock(&c->lock); \
                return CHAN_CLOSED; \
            } \
            mtx_unlock(&c->lock); \
 \
            if (timeout == NULL) { \
                evt_wait(&trg); \
            } else if (!evt_timedwait(&trg, timeout)) { \
                mtx_lock(&c->lock); \
                bool onqueue = waitq_remove((chan_waiter_ *)&w); \
                mtx_unlock(&c->lock); \
                if (!onqueue) { \
                    evt_wait(w.trg); \
                    chan_rc rc = buf_tryrecv##T(c, msg); \
                    if (rc != CHAN_WBLOCK) { \
                        return rc; \
                    } \
                } \
                return CHAN_WBLOCK; \
            } \
        } \
    }
CHAN_DEF_ALL_(BUF_OP_DECL, )

static chan_rc
unbuf_rendez_or_wait(
    chan_unbuf_ *c, void *msg, chan_waiter_unbuf_ *w, chan_op op
) {
    chan_waiter_root_ *shiftq, *pushq;
    if (op == CHAN_SEND) {
        shiftq = &c->recvq;
        pushq = &c->sendq;
    } else {
        shiftq = &c->sendq;
        pushq = &c->recvq;
    }

    while (xget_acq(&c->openc) > 0) {
        mtx_lock(&c->lock);
        if (xget_acq(&c->openc) == 0) {
            mtx_unlock(&c->lock);
            return CHAN_CLOSED;
        }

        chan_waiter_unbuf_ *w1 = &waitq_shift(shiftq)->unbuf;
        if (w1) {
            mtx_unlock(&c->lock);
            if (w1->alt_state) {
                size_t magic = ALT_MAGIC;
                if (!xcas_s_rlx_rlx(w1->alt_state, &magic, w1->alt_id)) {
                    xset_rel(&w1->ref, false);
                    continue;
                }
            }
            if (op == CHAN_SEND) {
                memcpy(w1->msg, msg, c->msgsz);
            } else {
                memcpy(msg, w1->msg, c->msgsz);
            }
            evt_post(w1->trg);
            return CHAN_OK;
        }
        waitq_push(pushq, (chan_waiter_ *)w);
        mtx_unlock(&c->lock);
        return CHAN_WBLOCK;
    }
    return CHAN_CLOSED;
}

static chan_rc
unbuf_rendez(
    chan_unbuf_ *c, void *msg, const struct timespec *timeout, chan_op op
) {
    evt trg = evt_make();
    chan_waiter_unbuf_ w = {.trg = &trg, .alt_id = ALT_NIL, .msg = msg};
    chan_rc rc = unbuf_rendez_or_wait(c, msg, &w, op);
    if (rc != CHAN_WBLOCK) {
        return rc;
    }

    if (timeout == NULL) {
        evt_wait(&trg);
    } else if (!evt_timedwait(&trg, timeout)) {
        mtx_lock(&c->lock);
        bool onqueue = waitq_remove((chan_waiter_ *)&w);
        mtx_unlock(&c->lock);
        if (onqueue) {
            return CHAN_WBLOCK;
        }
        evt_wait(w.trg);
    }
    return w.closed ? CHAN_CLOSED : CHAN_OK;
}

static struct timespec
usec_to_timespec(uint64_t timeout) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += (timeout % 1000000) * 1000;
    ts.tv_sec += (ts.tv_nsec / 1000000000) + (timeout / 1000000);
    ts.tv_nsec %= 1000000000;
    return ts;
}

#define CHAN_OP_DECL(T, _a) \
    chan_rc \
    chan_send_##T##_(chan_ *c, T msg) { \
        return c->hdr.cap > 0 ? \
            buf_send_##T(&c->buf_##T, msg, NULL) : \
            unbuf_rendez(&c->unbuf, &msg, NULL, CHAN_SEND); \
    } \
 \
    chan_rc \
    chan_recv_##T##_(chan_ *c, void *msg) { \
        return c->hdr.cap > 0 ? \
            buf_recv_##T(&c->buf_##T, msg, NULL) : \
            unbuf_rendez(&c->unbuf, msg, NULL, CHAN_RECV); \
    }
CHAN_DEF_ALL_(CHAN_OP_DECL, )

#define CHAN_TRYOP_DECL(T, _a) \
    chan_rc \
    chan_trysend_##T##_(chan_ *c, T msg) { \
        return c->hdr.cap > 0 ? \
            buf_trysend##T(&c->buf_##T, msg) : \
            unbuf_try(&c->unbuf, &msg, &c->unbuf.recvq); \
    } \
 \
    chan_rc \
    chan_tryrecv_##T##_(chan_ *c, void *msg) { \
        return c->hdr.cap > 0 ? \
            buf_tryrecv##T(&c->buf_##T, msg) : \
            unbuf_try(&c->unbuf, msg, &c->unbuf.sendq); \
    }
CHAN_DEF_ALL_(CHAN_TRYOP_DECL, )

#define CHAN_TIMEDOP_DECL(T, _a) \
    chan_rc \
    chan_timedsend_##T##_(chan_ *c, T msg, uint64_t timeout) { \
        struct timespec ts = usec_to_timespec(timeout); \
        return c->hdr.cap > 0 ? \
            buf_send_##T(&c->buf_##T, msg, &ts) : \
            unbuf_rendez(&c->unbuf, &msg, &ts, CHAN_SEND); \
    } \
 \
    chan_rc \
    chan_timedrecv_##T##_(chan_ *c, void *msg, uint64_t timeout) { \
        struct timespec ts = usec_to_timespec(timeout); \
        return c->hdr.cap > 0 ? \
            buf_recv_##T(&c->buf_##T, msg, &ts) : \
            unbuf_rendez(&c->unbuf, msg, &ts, CHAN_RECV); \
    }
CHAN_DEF_ALL_(CHAN_TIMEDOP_DECL, )

#define CHAN_ALT_DECL(T, _a) \
    chan_rc \
    chan_alt_try_##T##_(const chan_case *cc) { \
        return cc->op == CHAN_SEND ? \
            chan_trysend_##T##_(cc->c, *(T *)cc->msg) : \
            chan_tryrecv_##T##_(cc->c, cc->msg); \
    } \
 \
    bool \
    chan_alt_check_##T##_(chan_ *c, chan_op op) { \
        if (c->hdr.cap == 0) { \
            return op == CHAN_SEND ? /* Could just do the operation here? */ \
                &xget_rlx(&c->unbuf.recvq.next)->root != &c->unbuf.recvq : \
                &xget_rlx(&c->unbuf.sendq.next)->root != &c->unbuf.sendq; \
        } \
        chan_un64_ u = op == CHAN_SEND ? \
            (const chan_un64_){xget_rlx(&c->buf_##T.write.u64)} : \
            (const chan_un64_){xget_rlx(&c->buf_##T.read.u64)}; \
        chan_cell_(T) *cell = c->buf_##T.buf + u.idx; \
        return (int32_t)(u.lap - xget_rlx(&cell->lap)) <= 0; \
    }
CHAN_DEF_ALL_(CHAN_ALT_DECL, )

size_t
chan_tryalt_(const chan_case cases[], size_t len, size_t offset) {
    size_t closedc = 0;
    for (size_t i = 0; i < len; i++) {
        const chan_case *cc = cases + ((i + offset) % len);
        if (cc->op == CHAN_NOOP) {
            closedc++;
            continue;
        }
        switch (cc->fns.try(cc)) {
        case CHAN_OK: return (i + offset) % len;
        case CHAN_WBLOCK: break;
        case CHAN_CLOSED: closedc++;
        }
    }
    return closedc == len ? CHAN_CLOSED : CHAN_WBLOCK;
}

static enum {
    ALT_READY,
    ALT_WAIT,
    ALT_CLOSED,
}
alt_ready_or_wait(
    chan_case cases[static 1],
    size_t len,
    size_t offset,
    evt *trg,
    _Atomic size_t *state
) {
    size_t closedc = 0;
    for (size_t i = 0; i < len; i++) {
        chan_case *cc = cases + ((i + offset) % len);
        if (cc->op == CHAN_NOOP || xget_acq(&cc->c->hdr.openc) == 0) {
            closedc++;
            continue;
        }

        if (cc->c->hdr.cap == 0) {
            cc->w.unbuf.msg = cc->msg;
        }
        cc->w.hdr.trg = trg;
        cc->w.hdr.alt_state = state;
        cc->w.hdr.alt_id = (i + offset) % len;
        chan_waiter_root_ *waitq = cc->op == CHAN_SEND ?
            &cc->c->hdr.sendq : &cc->c->hdr.recvq;
        mtx_lock(&cc->c->hdr.lock);
        waitq_push(waitq, &cc->w);
        if (cc->fns.check(cc->c, cc->op)) {
            waitq_remove(&cc->w);
            mtx_unlock(&cc->c->hdr.lock);
            cc->w.hdr.trg = NULL;
            return ALT_READY;
        }
        mtx_unlock(&cc->c->hdr.lock);
    }
    return closedc == len ? ALT_CLOSED : ALT_WAIT;
}

static void
alt_remove_waiters(chan_case cases[static 1], size_t len, size_t state) {
    for (size_t i = 0; i < len; i++) {
        chan_case *cc = cases + i;
        if (
            cc->op == CHAN_NOOP ||
            xget_acq(&cc->c->hdr.openc) == 0 ||
            !cc->w.hdr.trg
        ) {
            continue;
        }

        mtx_lock(&cc->c->hdr.lock);
        bool onqueue = waitq_remove(&cc->w);
        mtx_unlock(&cc->c->hdr.lock);
        if (!onqueue && i != state) {
            while (xget_acq(&cc->w.hdr.ref)) {
                sched_yield();
            }
        }
        cc->w.hdr.trg = NULL;
    }
}

size_t
chan_alt_(chan_case cases[], size_t len, uint64_t timeout) {
    struct timespec ts;
    if (timeout > 0) {
        ts = usec_to_timespec(timeout);
    }
    size_t offset = rand();
    evt trg = evt_make();
    bool timedout = false;
    do {
        size_t idx = chan_tryalt_(cases, len, offset);
        if (idx != CHAN_WBLOCK) {
            return idx;
        }

        _Atomic size_t state;
        xset_rlx(&state, ALT_MAGIC);
        size_t state1 = ALT_MAGIC;
        switch (alt_ready_or_wait(cases, len, offset, &trg, &state)) {
        case ALT_CLOSED: return CHAN_CLOSED;
        case ALT_READY:
            if (!xcas_s_rlx_rlx(&state, &state1, ALT_NIL)) {
                evt_wait(&trg);
            }
            break;
        case ALT_WAIT:
            if (timeout == 0) {
                evt_wait(&trg);
            } else if (!evt_timedwait(&trg, &ts)) {
                timedout = true;
                if (!xcas_s_rlx_rlx(&state, &state1, ALT_NIL)) {
                    evt_wait(&trg);
                }
                break;
            }
            xcas_s_rlx_rlx(&state, &state1, ALT_NIL);
        }

        alt_remove_waiters(cases, len, state1);
        if (state1 != ALT_MAGIC) {
            chan_case *cc = cases + state1;
            if (cc->c->hdr.cap == 0 || cc->fns.try(cc) == CHAN_OK) {
                return state1;
            }
        }
    } while (!timedout);
    return CHAN_WBLOCK;
}
