#include <stdbool.h>
#include <stdlib.h>

#include <cee/cee.h>

#include <cee/map.h>

/* Everything past 29 is shamelessly stolen from "good hash table primes" by
 * akrowne on PlanetMath. */
static const size_t primes[] = {
    29, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317,
    196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843,
    50331653, 100663319, 201326611, 402653189, 805306457, 1610612741
};
static const size_t PRIMESLEN = sizeof(primes) / sizeof(primes[0]);

void *
map_make_(size_t bktsz) {
    map_hdr_ *m = malloc(sizeof(*m));
    *m = (const map_hdr_){0, calloc(primes[0], bktsz), 0, primes[0], 0};
    return m;
}

void *
map_drop_(map_hdr_ *m) {
    free(m->bkts);
    free(m);
    return NULL;
}

/* DJBX33X because I'm lazy. Replace with Murmur3 or w/e. Or not b/c icache? */
static uint32_t
djbx33x(const unsigned char *buf, size_t sz) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < sz; i++) {
        hash = (hash * 33) ^ buf[i];
    }
    return hash;
}

#define amp(a) (&a)
#define amp_str(a) (a)

#define size(a) (sizeof(a))
#define size_str(a) (strlen(a))

#define eq(a, b) ((a) == (b))
#define eq_str(a, b) (strcmp(a, b) == 0)

#define DEF_ALL(f) \
    f(map_kstr_, cee_u32_, amp_str, size_str, eq_str) \
    f(map_kstr_, cee_u64_, amp_str, size_str, eq_str) \
    f(map_kstr_, cee_u128_, amp_str, size_str, eq_str) \
    f(map_kstr_, cee_u192_, amp_str, size_str, eq_str) \
    f(map_kstr_, cee_u256_, amp_str, size_str, eq_str) \
    f(map_k32_, cee_u32_, amp, size, eq) \
    f(map_k32_, cee_u64_, amp, size, eq) \
    f(map_k32_, cee_u128_, amp, size, eq) \
    f(map_k32_, cee_u192_, amp, size, eq) \
    f(map_k32_, cee_u256_, amp, size, eq) \
    f(map_k64_, cee_u32_, amp, size, eq) \
    f(map_k64_, cee_u64_, amp, size, eq) \
    f(map_k64_, cee_u128_, amp, size, eq) \
    f(map_k64_, cee_u192_, amp, size, eq) \
    f(map_k64_, cee_u256_, amp, size, eq)

#define MAP_KEYS_DECL(K, V, amp, size, eq) \
    K * \
    map_keys_##K##_##V##_(map_(K, V) *m) { \
        K *keys = malloc(m->len * sizeof(*keys)); \
        size_t i = 0; \
        for (size_t j = 0; j < m->cap; j++) { \
            if (m->bkts[j].state == MAP_BKT_FULL_) { \
                cee_assert(i < m->len); \
                keys[i++] = m->bkts[j].key; \
            } \
        } \
        return keys; \
    }
DEF_ALL(MAP_KEYS_DECL)

#define RESIZE_DECL(K, V, amp_, size_, eq_) \
    static void \
    resize_##K##_##V(map_(K, V) *m, size_t primesidx) { \
        cee_assert(primesidx < PRIMESLEN); \
        size_t cap = primes[primesidx]; \
        map_bkt_(K, V) *bkts = calloc(cap, sizeof(*bkts)); \
        for (size_t i = 0; i < m->cap; i++) { \
            if (m->bkts[i].state == MAP_BKT_FULL_) { \
                for (size_t j = m->bkts[i].hash % cap; ; j = (j + 1) % cap) { \
                    if (bkts[j].state == MAP_BKT_EMPTY_) { \
                        bkts[j] = (const map_bkt_(K, V)){ \
                            m->bkts[i].key, \
                            m->bkts[i].val, \
                            MAP_BKT_FULL_, \
                            m->bkts[i].hash, \
                        }; \
                        break; \
                    } \
                } \
            } \
        } \
        free(m->bkts); \
        *m = (const map_(K, V)){m->len, bkts, m->len, cap, primesidx}; \
    }
DEF_ALL(RESIZE_DECL)

/* Linear probing because I'm lazy. Replace with robin hood hashing or w/e. */
#define MAP_REP_DECL(K, V, amp, size, eq) \
    bool \
    map_rep_##K##_##V##_(map_(K, V) *m, K key, V val, V *old_val) { \
        if ((double)m->used / m->cap > 0.75) {\
            resize_##K##_##V( \
                m, m->primesidx + ((double)m->len / m->cap > 0.3 ? 1 : 0)); \
        } \
        uint32_t hash = djbx33x((unsigned char *)amp(key), size(key)); \
        size_t putidx = SIZE_MAX; \
        for (size_t i = hash % m->cap; ; i = (i + 1) % m->cap) { \
            switch (m->bkts[i].state) { \
            case MAP_BKT_EMPTY_: \
                if (putidx == SIZE_MAX) { \
                    putidx = i; \
                    m->used++; \
                } \
                m->bkts[putidx] = \
                    (const map_bkt_(K, V)){key, val, MAP_BKT_FULL_, hash}; \
                m->len++; \
                return false; \
            case MAP_BKT_FULL_: \
                if (eq(key, m->bkts[i].key)) { \
                    if (old_val) { \
                        *old_val = m->bkts[i].val; \
                    } \
                    m->bkts[i] = \
                        (const map_bkt_(K, V)){ \
                            key, val, MAP_BKT_FULL_, hash \
                        }; \
                    m->used++; \
                    return true; \
                } \
                break; \
            case MAP_BKT_TOMB_: \
                if (putidx == SIZE_MAX) { \
                    putidx = i; \
                } \
            } \
        } \
    }
DEF_ALL(MAP_REP_DECL)

#define MAP_GET_DECL(K, V, amp, size, eq) \
    bool \
    map_get_##K##_##V##_(map_(K, V) *m, K key, V *val) { \
        uint32_t hash = djbx33x((unsigned char *)amp(key), size(key)); \
        for (size_t i = hash % m->cap; ; i = (i + 1) % m->cap) { \
            switch (m->bkts[i].state) { \
            case MAP_BKT_EMPTY_: \
                return false; \
            case MAP_BKT_FULL_: \
                if (eq(key, m->bkts[i].key)) { \
                    if (val) { \
                        *val = m->bkts[i].val; \
                    } \
                    return true; \
                } /* fallthrough */ \
            case MAP_BKT_TOMB_: break; \
            } \
        } \
    }
DEF_ALL(MAP_GET_DECL)

#define MAP_REM_DECL(K, V, amp, size, eq) \
    bool \
    map_rem_##K##_##V##_(map_(K, V) *m, K key, V *old_val) { \
        uint32_t hash = djbx33x((unsigned char *)amp(key), size(key)); \
        for (size_t i = hash % m->cap; ; i = (i + 1) % m->cap) { \
            switch (m->bkts[i].state) { \
            case MAP_BKT_EMPTY_: \
                return false; \
            case MAP_BKT_FULL_: \
                if (eq(key, m->bkts[i].key)) { \
                    m->bkts[i].state = MAP_BKT_TOMB_; \
                    if (old_val) { \
                        *old_val = m->bkts[i].val; \
                    } \
                    m->len--; \
                    if ((double)m->len / m->cap < 0.1 && m->primesidx > 0) { \
                        resize_##K##_##V(m, m->primesidx - 1); \
                    } \
                    return true; \
                } /* fallthrough */ \
            case MAP_BKT_TOMB_: break; \
            } \
        } \
        return true; \
    }
DEF_ALL(MAP_REM_DECL)
