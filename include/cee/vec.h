#ifndef CEE_VEC_H
#define CEE_VEC_H
#include <stdbool.h>
#include <stdlib.h>

#include <cee/cee.h>

#define vec(T) vec_paste_(T)
#define VEC_DEF(T) \
    typedef union vec(T) { \
        vec_ _vec; \
        struct { \
            T *arr; \
            size_t len, cap; \
        }; \
    } vec(T)
#define VEC_DEF_P(T) \
    P_DEF(T); \
    VEC_DEF(p(T))

#define vec_make(T, len, cap) \
    (const vec(T)){{vec_alloc_(sizeof(T), len, cap), len, cap}}
#define vec_drop(v) (free((v).arr), (const __typeof(v)){{0}})
#define vec_shrink(v) vec_shrink_(&(v)->_vec, sizeof(*(v)->arr))
#define vec_trim(v) vec_trim_(&(v)->_vec, sizeof(*(v)->arr))

#define vec_len(v) ((v)._vec.len)
#define vec_cap(v) ((v)._vec.cap)
#define vec_arr(v) ((v).arr)
#define vec_idx(v, idx) (*vec_idx_(v, idx, __COUNTER__))

#define vec_push(v, elt) vec_push_(v, elt, __COUNTER__)
#define vec_peek(v, elt) vec_peek_(v, elt, __COUNTER__)
#define vec_pop(v, elt) vec_pop_(v, elt, __COUNTER__)

#define vec_concat(v, v1) if (1) { \
    _Static_assert( \
        sizeof(*(v)->arr) == sizeof(*(v1).arr), \
        "incompatible vector element sizes"); \
    vec_concat_(&(v)->_vec, (v1)._vec, sizeof(*(v)->arr)); \
} else (void)0
#define vec_find(v, elt) vec_find_(v, elt, __COUNTER__)
#define vec_remove(v, idx) vec_remove_(&(v)->_vec, idx, sizeof(*(v)->arr))

/* ---------------------------- Implementation ---------------------------- */
#define vec_paste_(T) vec_##T##_

typedef struct vec_ {
    unsigned char *arr;
    size_t len, cap;
} vec_;

#define vec_idx_(v, idx, id) __extension__ ({ \
    __auto_type cee_sym_(v_, id) = v; \
    __auto_type cee_sym_(idx_, id) = idx; \
    cee_assert(cee_sym_(idx_, id) < (typeof(idx))cee_sym_(v_, id)._vec.len); \
    cee_sym_(v_, id).arr + cee_sym_(idx_, id); \
})

#define vec_push_(v, elt, id) if (1) { \
    __extension__ __auto_type cee_sym_(v_, id) = v; \
    if (cee_sym_(v_, id)->_vec.len == cee_sym_(v_, id)->_vec.cap) { \
        vec_grow_(&cee_sym_(v_, id)->_vec, sizeof(*(v)->arr)); \
    } \
    cee_sym_(v_, id)->arr[cee_sym_(v_, id)->_vec.len++] = elt; \
} else (void)0

#define vec_peek_(v, elt, id) __extension__ ({ \
    __label__ ret; \
    bool rc = true; \
    __auto_type cee_sym_(v_, id) = v; \
    if (cee_sym_(v_, id)._vec.len == 0) { \
        rc = false; \
        goto ret; \
    } \
    *elt = cee_sym_(v_, id).arr[cee_sym_(v_, id)._vec.len - 1]; \
ret: \
    rc; \
})

#define vec_pop_(v, elt, id) __extension__ ({ \
    __label__ ret; \
    __auto_type cee_sym_(v_, id) = v; \
    bool rc = true; \
    if (cee_sym_(v_, id)->_vec.len == 0) { \
        rc = false; \
        goto ret; \
    } \
    *elt = cee_sym_(v_, id)->arr[--cee_sym_(v_, id)->_vec.len]; \
ret: \
    rc; \
})

#define vec_find_(v, elt, id) __extension__ ({ \
    __label__ ret; \
    __auto_type cee_sym_(v_, id) = v; \
    __auto_type cee_sym_(elt_, id) = elt; \
    size_t cee_sym_(idx, id) = SIZE_MAX; \
    for (size_t i = 0; i < cee_sym_(v_, id)._vec.len; i++) { \
        if (cee_sym_(v_, id).arr[i] == cee_sym_(elt_, id)) { \
            cee_sym_(idx, id) = i; \
            goto ret; \
        } \
    } \
ret: \
    cee_sym_(idx, id); \
})

void *vec_alloc_(size_t, size_t, size_t);
void vec_shrink_(vec_ *, size_t);
void vec_trim_(vec_ *, size_t);
void vec_grow_(vec_ *, size_t);
void vec_concat_(vec_ *, vec_, size_t);
void vec_remove_(vec_ *, size_t, size_t);
#endif
