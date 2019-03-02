#ifndef CEE_MAP_H
#define CEE_MAP_H
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <cee/cee.h>

#define map(K, V) map_paste_(K, V)

#define MAP_DEF(K, V) \
    typedef union map(K, V) { \
        map_ _map; \
        size_t len; \
        union { \
            K *const k; \
            V *const v; \
        } _phantom; \
    } map(K, V)
#define MAP_DEF_VP(K, V) \
    P_DEF(V); \
    MAP_DEF(K, p(V))
#define MAP_DEF_PV(K, V) \
    P_DEF(K); \
    MAP_DEF(p(K), V)
#define MAP_DEF_PP(K, V) \
    P_DEF(K); \
    P_DEF(V); \
    MAP_DEF(p(K), p(V))

#define map_make(K, V) __extension__ ({ \
    _Static_assert( \
        MAP_ALL__(map_check_ksize_, K) false, "unsupported key size"); \
    _Static_assert( \
        MAP_ALL__(map_check_vsize_, V) false, "unsupported value size"); \
    (map(K, V) *)map_make_(map_bktsize_(K, V)); \
})
#define map_drop(m) map_drop_(&(m)->_map.hdr)

#define map_len(m) ((m)->len)
#define map_keys(m) \
    ((__typeof(m->_phantom.k))MAP_MATCH_( \
        *m->_phantom.k, *m->_phantom.v, map_keys_, m, *m->_phantom.k, , , ))

#define map_has(m, key) map_get(m, key, NULL)
#define map_put(m, key, val) map_rep(m, key, val, NULL)
#define map_get(m, key, val) __extension__ ({ \
    map_assert_compatible_(m, key, *val, *val); \
    MAP_MATCH_(key, *m->_phantom.v, map_get_, m, key, val, __COUNTER__, ); \
})
#define map_del(m, key) map_rem(m, key, NULL)
#define map_rep(m, key, val, old_val) __extension__ ({ \
    map_assert_compatible_(m, key, val, *old_val); \
    MAP_MATCH_(key, val, map_rep_, m, key, val, old_val, __COUNTER__); \
})
#define map_rem(m, key, old_val) __extension__ ({ \
    map_assert_compatible_(m, key, *old_val, *old_val); \
    MAP_MATCH_(key, *m->_phantom.v, map_rem_, m, key, old_val, __COUNTER_, ); \
})

/* ---------------------------- Implementation ---------------------------- */
#define map_paste_(K, V) map_##K##_##V##_

typedef char *map_kstr_;
typedef uint32_t map_k32_;
typedef uint64_t map_k64_;

#define MAP_ALL__STR_(f, a) \
    f(map_kstr_, cee_u32_, a) \
    f(map_kstr_, cee_u64_, a) \
    f(map_kstr_, cee_u128_, a) \
    f(map_kstr_, cee_u192_, a) \
    f(map_kstr_, cee_u256_, a)
#define MAP_ALL__(f, a) \
    f(map_k32_, cee_u32_, a) \
    f(map_k32_, cee_u64_, a) \
    f(map_k32_, cee_u128_, a) \
    f(map_k32_, cee_u192_, a) \
    f(map_k32_, cee_u256_, a) \
    f(map_k64_, cee_u32_, a) \
    f(map_k64_, cee_u64_, a) \
    f(map_k64_, cee_u128_, a) \
    f(map_k64_, cee_u192_, a) \
    f(map_k64_, cee_u256_, a)
#define MAP_ALL_(f, a) \
    MAP_ALL__STR_(f, a) \
    MAP_ALL__(f, a)
#define MAP_MATCH__STR_(l1, r, m, key, val, a, b) \
    __builtin_choose_expr(sizeof(l1) == sizeof(cee_u32_), \
        r(map_kstr_, cee_u32_, m, key, val, a, b), \
    __builtin_choose_expr(sizeof(l1) == sizeof(cee_u64_), \
        r(map_kstr_, cee_u64_, m, key, val, a, b), \
    __builtin_choose_expr(sizeof(l1) == sizeof(cee_u128_), \
        r(map_kstr_, cee_u128_, m, key, val, a, b), \
    __builtin_choose_expr(sizeof(l1) == sizeof(cee_u192_), \
        r(map_kstr_, cee_u192_, m, key, val, a, b), \
        r(map_kstr_, cee_u256_, m, key, val, a, b)))))
#define MAP_MATCH__(l, l1, r, m, key, val, a, b) \
    __builtin_choose_expr(sizeof(l) == sizeof(map_k32_), ( \
        __builtin_choose_expr(sizeof(l1) == sizeof(cee_u32_), \
            r(map_k32_, cee_u32_, m, key, val, a, b), \
        __builtin_choose_expr(sizeof(l1) == sizeof(cee_u64_), \
            r(map_k32_, cee_u64_, m, key, val, a, b), \
        __builtin_choose_expr(sizeof(l1) == sizeof(cee_u128_), \
            r(map_k32_, cee_u128_, m, key, val, a, b), \
        __builtin_choose_expr(sizeof(l1) == sizeof(cee_u192_), \
            r(map_k32_, cee_u192_, m, key, val, a, b), \
            r(map_k32_, cee_u256_, m, key, val, a, b))))) \
    ),( \
        __builtin_choose_expr(sizeof(l1) == sizeof(cee_u32_), \
            r(map_k64_, cee_u32_, m, key, val, a, b), \
        __builtin_choose_expr(sizeof(l1) == sizeof(cee_u64_), \
            r(map_k64_, cee_u64_, m, key, val, a, b), \
        __builtin_choose_expr(sizeof(l1) == sizeof(cee_u128_), \
            r(map_k64_, cee_u128_, m, key, val, a, b), \
        __builtin_choose_expr(sizeof(l1) == sizeof(cee_u192_), \
            r(map_k64_, cee_u192_, m, key, val, a, b), \
            r(map_k64_, cee_u256_, m, key, val, a, b))))) \
    ))
#define MAP_MATCH_(l, l1, r, m, key, val, a, b) \
    _Generic((key), \
    char *: MAP_MATCH__STR_(l1, r, m, key, val, a, b), \
    default: MAP_MATCH__(l, l1, r, m, key, val, a, b) \
    )

typedef struct map_hdr_ {
    size_t len;
    unsigned char *bkts;
    size_t used, cap, sizeidx;
} map_hdr_;

typedef enum map_bkt_state_ {
    MAP_BKT_EMPTY_,
    MAP_BKT_FULL_,
    MAP_BKT_TOMB_,
} map_bkt_state_;

#define map_bkt_(K, V) map_bkt_##K##_##V##_
#define MAP_BKT_DECL_(K, V, a_) \
    typedef struct map_bkt_(K, V) { \
        K key; \
        V val; \
        map_bkt_state_ state; \
        uint32_t hash; \
    } map_bkt_(K, V);
MAP_ALL_(MAP_BKT_DECL_, )

#define map_bktsize__(K, V, m_, key_, val_, a_, b_) sizeof(map_bkt_(K, V))
#define map_bktsize_(K, V) (MAP_MATCH__(K, V, map_bktsize__, , , , , ))

#define map_(K, V) map_##K##_##V##__
#define MAP_DECL_(K, V, a_) \
    typedef struct map_(K, V) { \
        size_t len; \
        map_bkt_(K, V) *bkts; \
        size_t used, cap, sizeidx; \
    } map_(K, V);
MAP_ALL_(MAP_DECL_, )

#define MAP_MEMB_(K, V, a_) map_(K, V) K##_##V##_;

typedef union map_ {
    map_hdr_ hdr;
    MAP_ALL_(MAP_MEMB_, )
} map_;

#define map_check_ksize_(K, V, K1) sizeof(K) == sizeof(K1) ||
#define map_check_vsize_(K, V, V1) sizeof(V) == sizeof(V1) ||
#define map_assert_compatible_(m, key, val, val1) \
    _Static_assert( \
        _Generic((key), __typeof(*m->_phantom.k): true, default: false), \
        "incompatible map and key types"); \
    _Static_assert( \
        _Generic((val), __typeof(*m->_phantom.v): true, default: false) || \
        __builtin_types_compatible_p(void, __typeof(val)), \
        "incompatible map and value types"); \
    _Static_assert( \
        _Generic((val1), __typeof(*m->_phantom.v): true, default: false) || \
        __builtin_types_compatible_p(void, __typeof(val1)), \
        "incompatible map and value types")

#define map_keys_(K, V, m, key_, val_, a_, b_) \
    map_keys_##K##_##V##_(&(m)->_map.K##_##V##_)

#define map_rep_(K, V, m, key, val, old_val, id) \
    map_rep_##K##_##V##_( \
        &(m)->_map.K##_##V##_, \
        cee_to_(K, key, id), \
        cee_to_(V, val, id), \
        (V *)old_val)

#define map_get_(K, V, m, key, val, id, b_) \
    map_get_##K##_##V##_( \
        &(m)->_map.K##_##V##_, \
        cee_to_(K, key, id), \
        (V *)val)

#define map_rem_(K, V, m, key, old_val, id, b_) \
    map_rem_##K##_##V##_( \
        &(m)->_map.K##_##V##_, \
        cee_to_(K, key, id), \
        (V *)old_val)

#define MAP_FN_DECL_(K, V, a_) \
    K *map_keys_##K##_##V##_(map_(K, V) *); \
    bool map_rep_##K##_##V##_(map_(K, V) *, K, V, V*); \
    bool map_get_##K##_##V##_(map_(K, V) *, K, V*); \
    bool map_rem_##K##_##V##_(map_(K, V) *, K, V*);

void *map_make_(size_t);
void *map_drop_(map_hdr_ *m);
MAP_ALL_(MAP_FN_DECL_, )
#endif
