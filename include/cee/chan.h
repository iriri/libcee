#ifndef CEE_CHAN_H
#define CEE_CHAN_H
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <cee/cee.h>
#include <cee/evt.h>
#include <cee/mtx.h>

#define chan(T) chan_paste_(T)
#define CHAN_DEF(T) \
    typedef union chan(T) { \
        chan_ _chan; \
        struct { \
            uint32_t cap; \
            mtx _lock; \
            _Atomic uint32_t openc, refc; \
        }; \
        T *const _phantom; \
    } chan(T)
#define CHAN_DEF_P(T) \
    P_DEF(T); \
    CHAN_DEF(p(T))

__extension__ typedef enum chan_rc {
    CHAN_OK,
    CHAN_WBLOCK = SIZE_MAX - 1,
    CHAN_CLOSED,
} chan_rc;

typedef struct chan_case chan_case;

typedef enum chan_op {
    CHAN_NOOP,
    CHAN_SEND,
    CHAN_RECV,
} chan_op;

#define chan_make(T, cap) __extension__ ({ \
    _Static_assert(CHAN_DEF_ALL_( \
        chan_test_msgsz_, T) false, "unsupported message size"); \
    (chan(T) *)chan_make_(sizeof(T), cap, chan_cellsz_(T)); \
})
#define chan_dup(c) chan_dup_(c, __COUNTER__)
#define chan_drop(c) chan_drop_(&(c)->_chan)
#define chan_ptrdrop(c) \
    ((void)sizeof(**c->_phantom), chan_ptrdrop_(&(c)->_chan))
#define chan_fndrop(c, fn) CHAN_MATCH_(*c->_phantom, chan_fndrop_, c, fn, , )
#define chan_open(c) chan_open_(c, __COUNTER__)
#define chan_close(c) chan_close_(&(c)->_chan)

#define chan_cap(c) ((c)->cap)
#define chan_openc(c) ((c)->openc)
#define chan_refc(c) ((c)->refc)

#define chan_send(c, msg) \
    chan_check_(c, msg, CHAN_MATCH_(msg, chan_send_, c, msg, __COUNTER__, ))
#define chan_trysend(c, msg) \
    chan_check_(c, msg, CHAN_MATCH_(msg, chan_trysend_, c, msg, __COUNTER__, ))
#define chan_timedsend(c, msg, timeout) \
    chan_check_( \
        c, \
        msg, \
        CHAN_MATCH_(msg, chan_timedsend_, c, msg, timeout, __COUNTER__))

#define chan_recv(c, msg) \
    chan_check_(c, *msg, CHAN_MATCH_(*msg, chan_recv_, c, msg, , ))
#define chan_tryrecv(c, msg) \
    chan_check_(c, *msg, CHAN_MATCH_(*msg, chan_tryrecv_, c, msg, , ))
#define chan_timedrecv(c, msg, timeout) \
    chan_check_(c, *msg, CHAN_MATCH_(*msg, chan_timedrecv_, c, msg, timeout, ))

#define chan_case(c_, op_, msg_) \
    chan_check_( \
        c_, \
        *msg_, \
        (const chan_case){ \
            .c = &(c_)->_chan, \
            .msg = msg_, \
            .fns = chan_alt_fns_(*msg_), \
            .op = op_ \
        })
#define chan_alt(cases, len) chan_alt_(cases, len, 0)
#define chan_tryalt(cases, len) chan_tryalt_(cases, len, rand())
#define chan_timedalt(cases, len, timeout) chan_alt_(cases, len, timeout)

#define chan_select(len) if (1) { \
    chan_select_(len)
#define chan_tryselect(len) if (1) { \
    void *cee_sym_(chan_wblock_, ) = NULL; \
    chan_select_(len)
#define chan_timedselect(len, timeout) if (1) { \
    uint64_t cee_sym_(chan_timeout_, ) = timeout; \
    void *cee_sym_(chan_wblock_, ) = NULL; \
    chan_select_(len)

#define chan_case_send(c, msg, idx) chan_case_(c, msg, idx, CHAN_SEND)
#define chan_case_recv(c, msg, idx) chan_case_(c, msg, idx, CHAN_RECV)
#define chan_case_wblock() chan_case_rc_(wblock_)
#define chan_case_closed() chan_case_rc_(closed_)

#define chan_select_end \
    chan_select_end_( \
        size_t idx = chan_alt( \
            cee_sym_(chan_cases_, ), cee_sym_(chan_len_, )); \
        if (idx != CHAN_CLOSED) { \
            __extension__ ({ goto *cee_sym_(chan_labels_, )[idx]; }); \
        } \
        if (cee_sym_(chan_closed_, )) { \
            __extension__ ({ goto *cee_sym_(chan_closed_, ); }); \
        })
#define chan_tryselect_end \
    chan_select_end_(chan_select_end_switch_( \
        chan_tryalt(cee_sym_(chan_cases_, ), cee_sym_(chan_len_, ))))
#define chan_timedselect_end \
    chan_select_end_(chan_select_end_switch_( \
        chan_timedalt( \
            cee_sym_(chan_cases_, ), \
            cee_sym_(chan_len_, ), \
            cee_sym_(chan_timeout_, ))))

/* ---------------------------- Implementation ---------------------------- */
#define chan_paste_(T) chan_##T##_

#define CHAN_DEF_ALL_(f, a) \
    f(cee_u32_, a) \
    f(cee_u64_, a) \
    f(cee_u128_, a) \
    f(cee_u192_, a) \
    f(cee_u256_, a)
#define CHAN_MATCH_(l, r, c, arg, a, b) \
    sizeof(l) == sizeof(cee_u32_) ? r(cee_u32_, c, arg, a, b) : \
    sizeof(l) == sizeof(cee_u64_) ? r(cee_u64_, c, arg, a, b) : \
    sizeof(l) == sizeof(cee_u128_) ? r(cee_u128_, c, arg, a, b) : \
    sizeof(l) == sizeof(cee_u192_) ? r(cee_u192_, c, arg, a, b) : \
        r(cee_u256_, c, arg, a, b)

typedef struct chan_waiter_root_ {
    union chan_waiter_ *_Atomic next, *_Atomic prev;
} chan_waiter_root_;

typedef struct chan_waiter_hdr_ {
    union chan_waiter_ *next, *prev;
    evt *trg;
    _Atomic size_t *alt_state;
    size_t alt_id;
    _Atomic bool ref;
} chan_waiter_hdr_;

typedef chan_waiter_hdr_ chan_waiter_buf_;

typedef struct chan_waiter_unbuf_ {
    struct waiter_unbuf *next, *prev;
    evt *trg;
    _Atomic size_t *alt_state;
    size_t alt_id;
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

typedef struct chan_hdr_ {
    uint32_t cap;
    mtx lock;
    _Atomic uint32_t openc, refc;
    chan_waiter_root_ sendq, recvq;
} chan_hdr_;

typedef struct chan_unbuf_ {
    uint32_t cap;
    mtx lock;
    _Atomic uint32_t openc, refc;
    chan_waiter_root_ sendq, recvq;
    size_t msgsz;
} chan_unbuf_;

typedef union chan_aun64_ {
    _Atomic uint64_t u64;
    struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        _Atomic uint32_t idx, lap;
#else
        _Atomic uint32_t lap, idx;
#endif
    };
} chan_aun64_;

typedef union chan_un64_ {
    uint64_t u64;
    struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        uint32_t idx, lap;
#else
        uint32_t lap, idx;
#endif
    };
} chan_un64_;

#define chan_cell_(T) chan_cell_##T##_
#define CHAN_CELL_DECL_(T, _a) \
    typedef struct chan_cell_(T) { \
        _Atomic uint32_t lap; \
        T msg; \
    } chan_cell_(T);
CHAN_DEF_ALL_(CHAN_CELL_DECL_, )

#define chan_cellsz__(T, _c, _arg, _a, _b) sizeof(chan_cell_(T))
#define chan_cellsz_(T) (CHAN_MATCH_(T, chan_cellsz__, , , , ))

#define chan_buf_(T) chan_buf_##T##_
#define CHAN_BUF_DECL_(T, _a) \
    typedef struct chan_buf_(T) { \
        uint32_t cap; \
        mtx lock; \
        _Atomic uint32_t openc, refc; \
        chan_waiter_root_ sendq, recvq; \
        const unsigned char pad[64 - (2 * sizeof(chan_waiter_root_))]; \
        chan_aun64_ write; \
        const unsigned char pad1[64 - sizeof(chan_aun64_)]; \
        chan_aun64_ read; \
        const unsigned char pad2[64 - sizeof(chan_aun64_)]; \
        chan_cell_(T) buf[]; \
    } chan_buf_(T);
CHAN_DEF_ALL_(CHAN_BUF_DECL_, )

#define CHAN_BUF_MEMB_(T, _a) chan_buf_(T) buf_##T;

typedef union chan_ {
    chan_hdr_ hdr;
    chan_unbuf_ unbuf;
    CHAN_DEF_ALL_(CHAN_BUF_MEMB_, )
} chan_;

typedef struct chan_alt_fns_ {
    chan_rc (*try)(const chan_case *cc);
    bool (*check)(chan_ *c, chan_op op);
} chan_alt_fns_;

/* TODO: This struct is huge... */
struct chan_case {
    chan_ *c;
    void *msg;
    chan_alt_fns_ fns; // could be an index into a table
    chan_op op;
    chan_waiter_ w;
};

#define chan_test_msgsz_(T, T1) sizeof(T) == sizeof(T1) ||
#define chan_check_(c, msg, ...) __extension__ ({ \
    _Static_assert( \
        _Generic((msg), __typeof(*c->_phantom): true, default: false), \
        "incompatible channel and message types"); \
    __VA_ARGS__; \
})

#define chan_fndrop_(T, c, fn, _a, _b) \
    chan_fndrop_##T##_(&(c)->_chan, fn)

#define chan_send_(T, c, msg, id, _b) \
    chan_send_##T##_(&(c)->_chan, cee_to_(T, msg, id))
#define chan_trysend_(T, c, msg, id, _b) \
    chan_trysend_##T##_(&(c)->_chan, cee_to_(T, msg, id))
#define chan_timedsend_(T, c, msg, timeout, id) \
    chan_timedsend_##T##_(&(c)->_chan, cee_to_(T, msg, id), timeout)

#define chan_recv_(T, c, msg, _a, _b) chan_recv_##T##_(&(c)->_chan, msg)
#define chan_tryrecv_(T, c, msg, _a, _b) chan_tryrecv_##T##_(&(c)->_chan, msg)
#define chan_timedrecv_(T, c, msg, timeout, _b) \
    chan_timedrecv_##T##_(&(c)->_chan, msg, timeout)

#define chan_dup_(c, id) __extension__ ({ \
    __auto_type cee_sym_(c_, id) = c; \
    uint32_t prev = atomic_fetch_add_explicit( \
        &cee_sym_(c_, id)->_chan.hdr.refc, 1, memory_order_relaxed); \
    cee_assert(0 < prev && prev < UINT32_MAX); \
    cee_sym_(c_, id); \
})

#define chan_open_(c, id) __extension__ ({ \
    __auto_type cee_sym_(c_, id) = c; \
    uint32_t prev = atomic_fetch_add_explicit( \
        &cee_sym_(c_, id)->_chan.hdr.openc, 1, memory_order_relaxed); \
    cee_assert(0 < prev && prev < UINT32_MAX); \
    cee_sym_(c_, id); \
})

#define chan_alt_fns__(T, _c, _arg, _a, _b) \
    (const chan_alt_fns_){chan_alt_try_##T##_, chan_alt_check_##T##_}
#define chan_alt_fns_(msg) (CHAN_MATCH_(msg, chan_alt_fns__, , , , ))

#define chan_select_(len) \
    chan_case cee_sym_(chan_cases_, )[len]; \
    const size_t cee_sym_(chan_len_, ) = len; \
    void *cee_sym_(chan_closed_, ) = NULL; \
    __extension__ void *cee_sym_(chan_labels_, )[len] = {}; \
    if (0) {

#define chan_case_(c, msg, idx, op) \
        } \
        goto cee_sym_(chan_label_out_, ); \
    } \
    cee_sym_(chan_cases_, )[idx] = chan_case(c, op, msg); \
    cee_sym_(chan_labels_, )[idx] = \
        __extension__ &&cee_sym_(chan_label_, idx); \
    if (0) { \
        { \
cee_sym_(chan_label_, idx)

#define chan_case_rc_(rc) \
        } \
        goto cee_sym_(chan_label_out_, ); \
    } \
    cee_sym_(chan_##rc, ) = __extension__ &&cee_sym_(chan_label_##rc, ); \
    if (0) { \
        { \
cee_sym_(chan_label_##rc, )

#define chan_select_end_(...) \
        goto cee_sym_(chan_label_out_, ); \
    } \
    { \
        __VA_ARGS__; \
    } \
cee_sym_(chan_label_out_, ): \
    (void)0; \
} else (void)0

#define chan_select_end_switch_(...) \
        size_t idx = __VA_ARGS__; \
        switch (idx) { \
        case CHAN_WBLOCK: \
            if (cee_sym_(chan_wblock_, )) { \
                __extension__ ({ goto *cee_sym_(chan_wblock_, ); }); \
            } \
            break; \
        case CHAN_CLOSED: \
            if (cee_sym_(chan_closed_, )) { \
                __extension__ ({ goto *cee_sym_(chan_closed_, ); }); \
            } \
            break; \
        default: __extension__ ({ goto *cee_sym_(chan_labels_, )[idx]; }); \
        } \

#define CHAN_FN_DECL_(T, _a) \
    void *chan_fndrop_##T##_(chan_ *, void (*)(void *)); \
    chan_rc chan_send_##T##_(chan_ *, T); \
    chan_rc chan_recv_##T##_(chan_ *, void *); \
    chan_rc chan_trysend_##T##_(chan_ *, T); \
    chan_rc chan_tryrecv_##T##_(chan_ *, void *); \
    chan_rc chan_timedsend_##T##_(chan_ *, T, uint64_t); \
    chan_rc chan_timedrecv_##T##_(chan_ *, void *, uint64_t); \
    chan_rc chan_alt_try_##T##_(const chan_case *cc); \
    bool chan_alt_check_##T##_(chan_ *c, chan_op op);

void *chan_make_(size_t, size_t, size_t);
void *chan_drop_(chan_ *);
void *chan_ptrdrop_(chan_ *);
void *chan_close_(chan_ *);
CHAN_DEF_ALL_(CHAN_FN_DECL_, )
size_t chan_alt_(chan_case[], size_t, uint64_t);
size_t chan_tryalt_(const chan_case[], size_t, size_t);
#endif
