/* The semantics of this library are inspired by channels from the Go
 * programming language.
 *
 * The implementations of sending, receiving, and selection are based on
 * algorithms described in "Go channels on steroids" by Dmitry Vyukov. */
#include <errno.h>
#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <cee/cee.h>
#include <cee/evt.h>
#include <cee/mtx.h>
#include <cee/vec.h>
#include <cee/xops.h>

#include <cee/chan.h>

typedef struct chan_waiter_hdr_ {
    union chan_waiter_ *next, *prev;
    evt *trg;
    _Atomic size_t *sel_state;
    size_t sel_id;
    _Atomic bool ref;
} chan_waiter_hdr_;

/* Buffered waiters currently only use the fields in the shared header. */
typedef struct chan_waiter_hdr_ chan_waiter_buf_;

typedef struct chan_waiter_unbuf_ {
    struct chan_waiter_unbuf_ *next, *prev;
    evt *trg;
    _Atomic size_t *sel_state;
    size_t sel_id;
    _Atomic bool ref;
    void *msg;
    bool closed;
} chan_waiter_unbuf_;

typedef union chan_waiter_ {
    chan_waiter_root_ root;
    chan_waiter_hdr_ hdr;
    chan_waiter_buf_ buf;
    chan_waiter_unbuf_ unbuf;
} chan_waiter_;

/* This has gotten kind of bloated... */
struct chan_case_ {
    chan_ *c;
    void *msg;
    chan_select_fns_ fns;
    chan_op op;
    chan_waiter_ waiter;
};

VEC_DEF(chan_case_);

struct chan_set {
    vec(chan_case_) cases;
    evt trg;
};

typedef enum chan_select_rc_ {
    CHAN_SEL_RC_READY_,
    CHAN_SEL_RC_WAIT_,
    CHAN_SEL_RC_CLOSED_,
} chan_select_rc_;

const size_t CHAN_SEL_NIL_ = CHAN_WBLOCK;
const size_t CHAN_SEL_MAGIC_ = CHAN_CLOSED;

void *
chan_make_(uint32_t msgsize, size_t cap, size_t cellsize) {
    chan_ *c;
    if (cap == 0) {
        cee_assert((c = calloc(1, sizeof(c->unbuf))));
        c->unbuf.msgsize = msgsize;
    } else {
        cee_assert(cap <= SIZE_MAX / msgsize);
        cee_assert((c = calloc(
            1, offsetof(chan_buf_cee_u256__, buf) + (cap * cellsize))));
        c->buf_cee_u256__.cap = cap;
        xset_rlx(&c->buf_cee_u256__.read.lap, 1);
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
        mtx_lock(&c->hdr.lock); // lol
        mtx_unlock(&c->hdr.lock); // lol
        free(c); // fallthrough
    default: return NULL;
    }
}

static void
chan_waitq_push_(chan_waiter_root_ *waitq, chan_waiter_ *w) {
    w->hdr.next = (chan_waiter_ *)waitq;
    w->hdr.prev = xget_rlx(&waitq->prev);
    xset_seq(&xget_rlx(&waitq->prev)->root.next, w);
    xset_rlx(&waitq->prev, w);
}

static chan_waiter_ *
chan_waitq_shift_(chan_waiter_root_ *waitq) {
    chan_waiter_ *w = xget_rlx(&waitq->next);
    if (&w->root == waitq) {
        return NULL;
    }
    xset_seq(&w->hdr.prev->root.next, w->hdr.next);
    xset_rlx(&w->hdr.next->root.prev, w->hdr.prev);
    w->hdr.prev = NULL; // For chan_waitq_remove
    xset_rlx(&w->hdr.ref, true);
    return w;
}

static bool
chan_waitq_remove_(chan_waiter_ *w) {
    if (!w->hdr.prev) { // Already shifted off the front
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
            while ((w = &chan_waitq_shift_(&c->hdr.sendq)->buf)) {
                evt_post(w->trg);
            }
            while ((w = &chan_waitq_shift_(&c->hdr.recvq)->buf)) {
                evt_post(w->trg);
            }
        } else {
            chan_waiter_unbuf_ *w;
            while ((w = &chan_waitq_shift_(&c->unbuf.sendq)->unbuf)) {
                w->closed = true;
                evt_post(w->trg);
            }
            while ((w = &chan_waitq_shift_(&c->unbuf.recvq)->unbuf)) {
                w->closed = true;
                evt_post(w->trg);
            }
        }
        mtx_unlock(&c->hdr.lock); // fallthrough
    default: return NULL;
    }
}

static void
chan_buf_waitq_shift_(chan_waiter_root_ *waitq, mtx *lock) {
    while (&xget_seq(&waitq->next)->root != waitq) {
        mtx_lock(lock);
        chan_waiter_buf_ *w = &chan_waitq_shift_(waitq)->buf;
        mtx_unlock(lock);
        if (w) {
            if (w->sel_state) {
                size_t magic = CHAN_SEL_MAGIC_;
                if (!xcas_s_acr_rlx(w->sel_state, &magic, w->sel_id)) {
                    xset_rel(&w->ref, false);
                    continue;
                }
            }
            evt_post(w->trg);
        }
        return;
    }
}

#define CHAN_BUF_TRYOP_DECL_(T, a_) \
    static chan_rc \
    chan_buf_trysend_##T##_(chan_buf_(T) *c, T msg) { \
        if (xget_acq(&c->openc) > 0) { \
            int i = 0; \
            chan_un64_ write = {xget_acq(&c->write.u64)}; \
            do { \
                chan_cell_(T) *cell = c->buf + write.idx; \
                uint32_t lap = xget_acq(&cell->lap); \
                if (write.lap == lap) { \
                    uint64_t write1 = write.idx + 1 < c->cap ? \
                            write.u64 + 1 : (uint64_t)(write.lap + 2) << 32; \
                    if (!xcas_w_seq_acq(&c->write.u64, &write.u64, write1)) { \
                        continue; \
                    } \
                    cell->msg = msg; \
                    xset_rel(&cell->lap, lap + 1); \
                    chan_buf_waitq_shift_(&c->recvq, &c->lock); \
                    return CHAN_OK; \
                } \
 \
                if (write.lap > lap) { \
                    if (++i > 4) { \
                        return CHAN_WBLOCK; \
                    } \
                    sched_yield(); \
                } \
                write.u64 = xget_acq(&c->write.u64); \
            } while (xget_acq(&c->openc) > 0); \
        } \
        return CHAN_CLOSED; \
    } \
 \
    static chan_rc \
    chan_buf_tryrecv_##T##_(chan_buf_(T) *c, T *msg) { \
        chan_un64_ read = {xget_acq(&c->read.u64)}; \
        for (int i = 0; ; ) { \
            chan_cell_(T) *cell = c->buf + read.idx; \
            uint32_t lap = xget_acq(&cell->lap); \
            if (read.lap == lap) { \
                uint64_t read1 = read.idx + 1 < c->cap ? \
                        read.u64 + 1 : (uint64_t)(read.lap + 2) << 32; \
                if (!xcas_w_seq_acq(&c->read.u64, &read.u64, read1)) { \
                    continue; \
                } \
                *msg = cell->msg; \
                xset_rel(&cell->lap, lap + 1); \
                chan_buf_waitq_shift_(&c->sendq, &c->lock); \
                return CHAN_OK; \
            } \
 \
            if (read.lap > lap) { \
                if (xget_acq(&c->openc) == 0) { \
                    return CHAN_CLOSED; \
                } \
                if (++i > 4) { \
                    return CHAN_WBLOCK; \
                } \
                sched_yield(); \
            } \
            read.u64 = xget_acq(&c->read.u64); \
        } \
    }
CHAN_ALL_(CHAN_BUF_TRYOP_DECL_, )

static chan_rc
chan_unbuf_try_(chan_unbuf_ *c, void *msg, chan_waiter_root_ *waitq) {
    while (xget_acq(&c->openc) > 0) {
        if (&xget_acq(&waitq->next)->root == waitq) {
            return CHAN_WBLOCK;
        }

        mtx_lock(&c->lock);
        if (xget_acq(&c->openc) == 0) {
            mtx_unlock(&c->lock);
            return CHAN_CLOSED;
        }
        chan_waiter_unbuf_ *w = &chan_waitq_shift_(waitq)->unbuf;
        mtx_unlock(&c->lock);
        if (!w) {
            return CHAN_WBLOCK;
        }
        if (w->sel_state) {
            size_t magic = CHAN_SEL_MAGIC_;
            if (!xcas_s_acr_rlx(w->sel_state, &magic, w->sel_id)) {
                xset_rel(&w->ref, false);
                continue;
            }
        }

        if (waitq == &c->recvq) {
            memcpy(w->msg, msg, c->msgsize);
        } else {
            memcpy(msg, w->msg, c->msgsize);
        }
        evt_post(w->trg);
        return CHAN_OK;
    }
    return CHAN_CLOSED;
}

#define CHAN_BUF_OP_OR_WAIT_DECL_(T, a_) \
    static chan_rc \
    chan_buf_send_or_wait_##T##_( \
        chan_buf_(T) *c, T msg, chan_waiter_buf_ *w \
    ) { \
        for ( ; ; ) { \
            chan_rc rc = chan_buf_trysend_##T##_(c, msg); \
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
            chan_waitq_push_(&c->sendq, (chan_waiter_ *)w); \
            chan_un64_ write = {xget_acq(&c->write.u64)}; \
            chan_cell_(T) *cell = c->buf + write.idx; \
            if (write.lap == xget_acq(&cell->lap)) { \
                chan_waitq_remove_((chan_waiter_ *)w); \
                mtx_unlock(&c->lock); \
                continue; \
            } \
            mtx_unlock(&c->lock); \
            return CHAN_WBLOCK; \
        } \
    } \
 \
    static chan_rc \
    chan_buf_recv_or_wait_##T##_( \
        chan_buf_(T) *c, T *msg, chan_waiter_buf_ *w \
    ) { \
        for ( ; ; ) { \
            chan_rc rc = chan_buf_tryrecv_##T##_(c, msg); \
            if (rc != CHAN_WBLOCK) { \
                return rc; \
            } \
 \
            mtx_lock(&c->lock); \
            chan_waitq_push_(&c->recvq, (chan_waiter_ *)w); \
            chan_un64_ read = {xget_acq(&c->read.u64)}; \
            chan_cell_(T) *cell = c->buf + read.idx; \
            if (read.lap == xget_acq(&cell->lap)) { \
                chan_waitq_remove_((chan_waiter_ *)w); \
                mtx_unlock(&c->lock); \
                continue; \
            } \
            if (xget_acq(&c->openc) == 0) { \
                chan_waitq_remove_((chan_waiter_ *)w); \
                mtx_unlock(&c->lock); \
                return CHAN_CLOSED; \
            } \
            mtx_unlock(&c->lock); \
            return CHAN_WBLOCK; \
        } \
    }
CHAN_ALL_(CHAN_BUF_OP_OR_WAIT_DECL_, )

#define CHAN_BUF_OP_DECL_(T, a_) \
    static chan_rc \
    chan_buf_send_##T##_(chan_buf_(T) *c, T msg, struct timespec *timeout) { \
        evt trg = evt_make(); \
        chan_waiter_buf_ w = {.trg = &trg, .sel_id = CHAN_SEL_NIL_}; \
        for ( ; ; ) { \
            chan_rc rc = chan_buf_send_or_wait_##T##_(c, msg, &w); \
            if (rc != CHAN_WBLOCK) { \
                return rc; \
            } \
 \
            if (timeout == NULL) { \
                evt_wait(&trg); \
            } else if (!evt_timedwait(&trg, timeout)) { \
                mtx_lock(&c->lock); \
                bool onqueue = chan_waitq_remove_((chan_waiter_ *)&w); \
                mtx_unlock(&c->lock); \
                if (!onqueue) { \
                    evt_wait(w.trg); \
                    chan_rc rc = chan_buf_trysend_##T##_(c, msg); \
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
    chan_buf_recv_##T##_(chan_buf_(T) *c, T *msg, struct timespec *timeout) { \
        evt trg = evt_make(); \
        chan_waiter_buf_ w = {.trg = &trg, .sel_id = CHAN_SEL_NIL_}; \
        for ( ; ; ) { \
            chan_rc rc = chan_buf_recv_or_wait_##T##_(c, msg, &w); \
            if (rc != CHAN_WBLOCK) { \
                return rc; \
            } \
 \
            if (timeout == NULL) { \
                evt_wait(&trg); \
            } else if (!evt_timedwait(&trg, timeout)) { \
                mtx_lock(&c->lock); \
                bool onqueue = chan_waitq_remove_((chan_waiter_ *)&w); \
                mtx_unlock(&c->lock); \
                if (!onqueue) { \
                    evt_wait(w.trg); \
                    chan_rc rc = chan_buf_tryrecv_##T##_(c, msg); \
                    if (rc != CHAN_WBLOCK) { \
                        return rc; \
                    } \
                } \
                return CHAN_WBLOCK; \
            } \
        } \
    }
CHAN_ALL_(CHAN_BUF_OP_DECL_, )

static chan_rc
chan_unbuf_rendez_or_wait_(
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

        chan_waiter_unbuf_ *w1 = &chan_waitq_shift_(shiftq)->unbuf;
        if (w1) {
            mtx_unlock(&c->lock);
            if (w1->sel_state) {
                size_t magic = CHAN_SEL_MAGIC_;
                if (!xcas_s_acr_rlx(w1->sel_state, &magic, w1->sel_id)) {
                    xset_rel(&w1->ref, false);
                    continue;
                }
            }
            if (op == CHAN_SEND) {
                memcpy(w1->msg, msg, c->msgsize);
            } else {
                memcpy(msg, w1->msg, c->msgsize);
            }
            evt_post(w1->trg);
            return CHAN_OK;
        }
        chan_waitq_push_(pushq, (chan_waiter_ *)w);
        mtx_unlock(&c->lock);
        return CHAN_WBLOCK;
    }
    return CHAN_CLOSED;
}

static chan_rc
chan_unbuf_rendez_(
    chan_unbuf_ *c, void *msg, struct timespec *timeout, chan_op op
) {
    evt trg = evt_make();
    chan_waiter_unbuf_ w = {.trg = &trg, .sel_id = CHAN_SEL_NIL_, .msg = msg};
    chan_rc rc = chan_unbuf_rendez_or_wait_(c, msg, &w, op);
    if (rc != CHAN_WBLOCK) {
        return rc;
    }

    if (timeout == NULL) {
        evt_wait(&trg);
    } else if (!evt_timedwait(&trg, timeout)) {
        mtx_lock(&c->lock);
        bool onqueue = chan_waitq_remove_((chan_waiter_ *)&w);
        mtx_unlock(&c->lock);
        if (onqueue) {
            return CHAN_WBLOCK;
        }
        evt_wait(w.trg);
    }
    return w.closed ? CHAN_CLOSED : CHAN_OK;
}

#define CHAN_OP_DECL_(T, a_) \
    chan_rc \
    chan_send_##T##_(chan_ *c, T msg) { \
        return c->hdr.cap > 0 ? \
                chan_buf_send_##T##_(&c->buf_##T##_, msg, NULL) : \
                chan_unbuf_rendez_(&c->unbuf, &msg, NULL, CHAN_SEND); \
    } \
 \
    chan_rc \
    chan_recv_##T##_(chan_ *c, void *msg) { \
        return c->hdr.cap > 0 ? \
                chan_buf_recv_##T##_(&c->buf_##T##_, msg, NULL) : \
                chan_unbuf_rendez_(&c->unbuf, msg, NULL, CHAN_RECV); \
    }
CHAN_ALL_(CHAN_OP_DECL_, )

#define CHAN_TRYOP_DECL_(T, a_) \
    chan_rc \
    chan_trysend_##T##_(chan_ *c, T msg) { \
        return c->hdr.cap > 0 ? \
                chan_buf_trysend_##T##_(&c->buf_##T##_, msg) : \
                chan_unbuf_try_(&c->unbuf, &msg, &c->unbuf.recvq); \
    } \
 \
    chan_rc \
    chan_tryrecv_##T##_(chan_ *c, void *msg) { \
        return c->hdr.cap > 0 ? \
                chan_buf_tryrecv_##T##_(&c->buf_##T##_, msg) : \
                chan_unbuf_try_(&c->unbuf, msg, &c->unbuf.sendq); \
    }
CHAN_ALL_(CHAN_TRYOP_DECL_, )

#define CHAN_TIMEDOP_DECL_(T, a_) \
    chan_rc \
    chan_timedsend_##T##_(chan_ *c, T msg, uint64_t timeout) { \
        struct timespec ts = ftx_rel_to_abs(timeout); \
        return c->hdr.cap > 0 ? \
                chan_buf_send_##T##_(&c->buf_##T##_, msg, &ts) : \
                chan_unbuf_rendez_(&c->unbuf, &msg, &ts, CHAN_SEND); \
    } \
 \
    chan_rc \
    chan_timedrecv_##T##_(chan_ *c, void *msg, uint64_t timeout) { \
        struct timespec ts = ftx_rel_to_abs(timeout); \
        return c->hdr.cap > 0 ? \
                chan_buf_recv_##T##_(&c->buf_##T##_, msg, &ts) : \
                chan_unbuf_rendez_(&c->unbuf, msg, &ts, CHAN_RECV); \
    }
CHAN_ALL_(CHAN_TIMEDOP_DECL_, )

chan_set *
chan_set_make_(size_t cap) {
    chan_set *s = malloc(sizeof(*s));
    cee_assert(s);
    s->cases = vec_make(chan_case_, 0, cap);
    xset_rlx(&s->trg, (evt){0});
    return s;
}

chan_set *
chan_set_drop_(chan_set *s) {
    vec_drop(s->cases);
    free(s);
    return NULL;
}

size_t
chan_set_add_(
    chan_set *s, chan_ *c, chan_op op, void *msg, chan_select_fns_ fns
) {
    vec_push(
        &s->cases, ((chan_case_){.c = c, .msg = msg, .op = op, .fns = fns}));
    cee_assert(s->cases.len - 1 <= RAND_MAX);
    return s->cases.len - 1;
}

void
chan_set_rereg_(chan_set *s, size_t id, chan_op op, void *msg) {
    vec_idx(s->cases, id).op = op;
    s->cases.arr[id].msg = msg;
}

#define CHAN_SELECT_DECL_(T, a_) \
    chan_rc \
    chan_select_try_##T##_(chan_case_ *cc) { \
        return cc->op == CHAN_SEND ? \
            chan_trysend_##T##_(cc->c, *(T *)cc->msg) : \
            chan_tryrecv_##T##_(cc->c, cc->msg); \
    } \
 \
    bool \
    chan_select_check_##T##_(chan_ *c, chan_op op) { \
        if (c->hdr.cap == 0) { \
            return op == CHAN_SEND ? /* Could just do the operation here? */ \
                &xget_acq(&c->unbuf.recvq.next)->root != &c->unbuf.recvq : \
                &xget_acq(&c->unbuf.sendq.next)->root != &c->unbuf.sendq; \
        } \
 \
        chan_un64_ u = op == CHAN_SEND ? \
            (chan_un64_){xget_acq(&c->buf_##T##_.write.u64)} : \
            (chan_un64_){xget_acq(&c->buf_##T##_.read.u64)}; \
        chan_cell_(T) *cell = c->buf_##T##_.buf + u.idx; \
        return u.lap == xget_acq(&cell->lap); \
    }
CHAN_ALL_(CHAN_SELECT_DECL_, )

static size_t
chan_select_test_all_(chan_set *s, size_t offset) {
    for (size_t i = 0; i < s->cases.len; i++) {
        chan_case_ *cc = s->cases.arr + ((i + offset) % s->cases.len);
        if (cc->fns.try(cc) == CHAN_OK) {
            return (i + offset) % s->cases.len;
        }
    }
    return CHAN_SEL_NIL_;
}

static chan_select_rc_
chan_select_ready_or_wait_(
    chan_set *s, _Atomic size_t *state, size_t offset
) {
    size_t closedc = 0;
    for (size_t i = 0; i < s->cases.len; i++) {
        chan_case_ *cc = s->cases.arr + ((i + offset) % s->cases.len);
        if (cc->op == CHAN_NOOP || xget_acq(&cc->c->hdr.openc) == 0) {
            closedc++;
            continue;
        }

        if (cc->c->hdr.cap == 0) {
            cc->waiter.unbuf.msg = cc->msg;
        }
        cc->waiter.hdr.trg = &s->trg;
        cc->waiter.hdr.sel_state = state;
        cc->waiter.hdr.sel_id = (i + offset) % s->cases.len;
        chan_waiter_root_ *waitq = cc->op == CHAN_SEND ?
                &cc->c->hdr.sendq : &cc->c->hdr.recvq;
        mtx_lock(&cc->c->hdr.lock);
        chan_waitq_push_(waitq, &cc->waiter);
        if (cc->fns.check(cc->c, cc->op)) {
            chan_waitq_remove_(&cc->waiter);
            mtx_unlock(&cc->c->hdr.lock);
            cc->waiter.hdr.trg = NULL;
            return CHAN_SEL_RC_READY_;
        }
        mtx_unlock(&cc->c->hdr.lock);
    }

    if (closedc == s->cases.len) {
        return CHAN_SEL_RC_CLOSED_;
    }
    return CHAN_SEL_RC_WAIT_;
}

static void
chan_select_remove_waiters_(chan_set *s, size_t state) {
    for (size_t i = 0; i < s->cases.len; i++) {
        chan_case_ *cc = s->cases.arr + i;
        if (
            cc->op == CHAN_NOOP ||
            xget_acq(&cc->c->hdr.openc) == 0 ||
            !cc->waiter.hdr.trg
        ) {
            continue;
        }
        mtx_lock(&cc->c->hdr.lock);
        bool onqueue = chan_waitq_remove_(&cc->waiter);
        mtx_unlock(&cc->c->hdr.lock);
        if (!onqueue && i != state) {
            while (xget_acq(&cc->waiter.hdr.ref)) {
                sched_yield();
            }
        }
        cc->waiter.hdr.trg = NULL;
    }
}

size_t
chan_select_(chan_set *s, uint64_t timeout) {
    size_t offset = rand() % s->cases.len;
    struct timespec ts;
    if (timeout == 0) {
        return chan_select_test_all_(s, offset);
    }
    if (timeout < UINT64_MAX) {
        ts = ftx_rel_to_abs(timeout);
    }

    bool timedout;
    do {
        size_t rc = chan_select_test_all_(s, offset);
        if (rc != CHAN_SEL_NIL_) {
            return rc;
        }

        _Atomic size_t state;
        size_t magic = CHAN_SEL_MAGIC_;
        timedout = false;
        xset_rlx(&state, magic);
        switch (chan_select_ready_or_wait_(s, &state, offset)) {
        case CHAN_SEL_RC_CLOSED_: return CHAN_CLOSED;
        case CHAN_SEL_RC_READY_:
            xcas_s_acr_rlx(&state, &magic, CHAN_SEL_NIL_);
            if (state < CHAN_SEL_NIL_) {
                evt_wait(&s->trg);
            }
            break;
        case CHAN_SEL_RC_WAIT_:
            if (timeout == UINT64_MAX) {
                evt_wait(&s->trg);
            } else if (!evt_timedwait(&s->trg, &ts)) {
                timedout = true;
                xcas_s_acr_rlx(&state, &magic, CHAN_SEL_NIL_);
                if (state < CHAN_SEL_NIL_) {
                    evt_wait(&s->trg);
                }
                break;
            }
            xcas_s_acr_rlx(&state, &magic, CHAN_SEL_NIL_);
        }

        chan_select_remove_waiters_(s, magic);
        if (state < CHAN_SEL_NIL_) {
            chan_case_ *cc = s->cases.arr + state;
            if (cc->c->hdr.cap == 0 || cc->fns.try(cc) == CHAN_OK) {
                return state;
            }
        }
    } while (!timedout);
    return CHAN_WBLOCK;
}
