#ifndef CEE_CHAN_H
#define CEE_CHAN_H
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <cee/cee.h>
#include <cee/mtx.h>

#define chan(T) chan_paste_(T)
#define CHAN_DEF(T) \
    typedef union chan(T) { \
        chan_ _chan; \
        uint32_t cap; \
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
        chan_check_msgsize_, T) false, "unsupported message size"); \
    (chan(T) *)chan_make_(sizeof(T), cap, chan_cellsize_(T)); \
})
#define chan_dup(c) chan_dup_(c, __COUNTER__)
#define chan_drop(c) chan_drop_(&(c)->_chan)
#define chan_open(c) chan_open_(c, __COUNTER__)
#define chan_close(c) chan_close_(&(c)->_chan)

#define chan_cap(c) ((c)->cap)

#define chan_send(c, msg) chan_send_(c, msg, , __COUNTER__)
#define chan_trysend(c, msg) chan_send_(c, msg, try, __COUNTER__)
#define chan_timedsend(c, msg, timeout) __extension__ ({ \
    chan_assert_compatible_(c, msg); \
    CHAN_MATCH_(msg, chan_timedsend_, c, msg, timeout, __COUNTER__); \
})

#define chan_recv(c, msg) chan_recv_(c, msg, )
#define chan_tryrecv(c, msg) chan_recv_(c, msg, try)
#define chan_timedrecv(c, msg, timeout) __extension__ ({ \
    chan_assert_compatible_(c, *msg); \
    CHAN_MATCH_(*msg, chan_timedrecv_, c, msg, timeout, ); \
})

#define chan_case(c, op, msg) __extension__ ({ \
    chan_assert_compatible_(c, *msg); \
    (chan_case){&(c)->_chan, msg, chan_select_fns_(msg), op}; \
})

#define chan_select(cases, len) chan_select_(cases, len, 0)
#define chan_tryselect(cases, len) chan_tryselect_(cases, len, rand())
#define chan_timedselect(cases, len, timeout) chan_select_(cases, len, timeout)

/* ---------------------------- Implementation ---------------------------- */
#define chan_paste_(T) chan_##T##_

#define CHAN_DEF_ALL_(f, a) \
    f(cee_u32_, a) \
    f(cee_u64_, a) \
    f(cee_u128_, a) \
    f(cee_u192_, a) \
    f(cee_u256_, a)
#define CHAN_MATCH_(l, r, c, msg, a, b) \
    sizeof(l) == sizeof(cee_u32_) ? r(cee_u32_, c, msg, a, b) : \
    sizeof(l) == sizeof(cee_u64_) ? r(cee_u64_, c, msg, a, b) : \
    sizeof(l) == sizeof(cee_u128_) ? r(cee_u128_, c, msg, a, b) : \
    sizeof(l) == sizeof(cee_u192_) ? r(cee_u192_, c, msg, a, b) : \
        r(cee_u256_, c, msg, a, b)

typedef struct chan_waiter_root_ {
    union chan_waiter_ *_Atomic next, *_Atomic prev;
} chan_waiter_root_;

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
    size_t msgsize;
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

#define CHAN_CELL_DECL_(T, a_) \
    typedef struct chan_cell_(T) { \
        _Atomic uint32_t lap; \
        T msg; \
    } chan_cell_(T);
CHAN_DEF_ALL_(CHAN_CELL_DECL_, )

#define chan_cellsize__(T, c_, msg_, a_, b_) sizeof(chan_cell_(T))
#define chan_cellsize_(T) (CHAN_MATCH_(T, chan_cellsize__, , , , ))

#define chan_buf_(T) chan_buf_##T##_
#define CHAN_BUF_DECL_(T, a_) \
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

#define CHAN_BUF_MEMB_(T, a_) chan_buf_(T) buf_##T##_;

typedef union chan_ {
    chan_hdr_ hdr;
    chan_unbuf_ unbuf;
    CHAN_DEF_ALL_(CHAN_BUF_MEMB_, )
} chan_;

typedef struct chan_select_fns_ {
    chan_rc (*try)(const chan_case *cc);
    bool (*check)(chan_ *c, chan_op op);
} chan_select_fns_;

struct chan_case {
    chan_ *c;
    void *msg;
    chan_select_fns_ fns;
    chan_op op;
};

#define chan_check_msgsize_(T, T1) sizeof(T) == sizeof(T1) ||
#define chan_assert_compatible_(c, msg) \
    _Static_assert( \
        _Generic((msg), __typeof(*c->_phantom): true, default: false), \
        "incompatible channel and message types") \

#define chan_send__(T, c, msg, mod, id) \
    chan_##mod##send_##T##_(&(c)->_chan, cee_to_(T, msg, id))
#define chan_send_(c, msg, mod, id) __extension__ ({ \
    chan_assert_compatible_(c, msg); \
    CHAN_MATCH_(msg, chan_send__, c, msg, mod, id); \
})

#define chan_timedsend_(T, c, msg, timeout, id) \
    chan_timedsend_##T##_(&(c)->_chan, cee_to_(T, msg, id), timeout)

#define chan_recv__(T, c, msg, mod, b_) \
    chan_##mod##recv_##T##_(&(c)->_chan, msg)
#define chan_recv_(c, msg, mod) __extension__ ({ \
    chan_assert_compatible_(c, *msg); \
    CHAN_MATCH_(*msg, chan_recv__, c, msg, mod, ); \
})

#define chan_timedrecv_(T, c, msg, timeout, b_) \
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

#define chan_select_fns__(T, c_, msg_, a_, b_) \
    (chan_select_fns_){chan_select_try_##T##_, chan_select_check_##T##_}
#define chan_select_fns_(msg) (CHAN_MATCH_(msg, chan_select_fns__, , , , ))

#define CHAN_FN_DECL_(T, a_) \
    chan_rc chan_send_##T##_(chan_ *, T); \
    chan_rc chan_recv_##T##_(chan_ *, void *); \
    chan_rc chan_trysend_##T##_(chan_ *, T); \
    chan_rc chan_tryrecv_##T##_(chan_ *, void *); \
    chan_rc chan_timedsend_##T##_(chan_ *, T, uint64_t); \
    chan_rc chan_timedrecv_##T##_(chan_ *, void *, uint64_t); \
    chan_rc chan_select_try_##T##_(const chan_case *cc); \
    bool chan_select_check_##T##_(chan_ *c, chan_op op);

void *chan_make_(uint32_t, size_t, size_t);
void *chan_drop_(chan_ *);
void *chan_close_(chan_ *);
CHAN_DEF_ALL_(CHAN_FN_DECL_, )
size_t chan_select_(chan_case[static 1], size_t, uint64_t);
size_t chan_tryselect_(chan_case[static 1], size_t, size_t);
#endif
