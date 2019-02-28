#include <stdbool.h>
#include <stdlib.h>

#include <cee/cee.h>

#include <cee/map.h>

void *
map_drop_(map_ *m) {
    free(m->hdr.bkts);
    free(m);
    return NULL;
}

/* DJBX33X because I'm lazy. Replace with Murmur3 or SipHash or w/e. */
static uint32_t
map_hash_(unsigned char *buf, size_t size) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < size; i++) {
        hash = (hash * 33) ^ buf[i];
    }
    return hash;
}

#define map_amp_(a) (&a)
#define map_amp_str_(a) (a)

#define map_size_(a) (sizeof(a))
#define map_size_str_(a) (strlen(a))

#define map_eq_(a, b) ((a) == (b))
#define map_eq_str_(a, b) (strcmp(a, b) == 0)

#define MAP_ALL_IMPL_(f) \
    f(map_kstr_, cee_u32_, map_amp_str_, map_size_str_, map_eq_str_) \
    f(map_kstr_, cee_u64_, map_amp_str_, map_size_str_, map_eq_str_) \
    f(map_kstr_, cee_u128_, map_amp_str_, map_size_str_, map_eq_str_) \
    f(map_kstr_, cee_u192_, map_amp_str_, map_size_str_, map_eq_str_) \
    f(map_kstr_, cee_u256_, map_amp_str_, map_size_str_, map_eq_str_) \
    f(map_k32_, cee_u32_, map_amp_, map_size_, map_eq_) \
    f(map_k32_, cee_u64_, map_amp_, map_size_, map_eq_) \
    f(map_k32_, cee_u128_, map_amp_, map_size_, map_eq_) \
    f(map_k32_, cee_u192_, map_amp_, map_size_, map_eq_) \
    f(map_k32_, cee_u256_, map_amp_, map_size_, map_eq_) \
    f(map_k64_, cee_u32_, map_amp_, map_size_, map_eq_) \
    f(map_k64_, cee_u64_, map_amp_, map_size_, map_eq_) \
    f(map_k64_, cee_u128_, map_amp_, map_size_, map_eq_) \
    f(map_k64_, cee_u192_, map_amp_, map_size_, map_eq_) \
    f(map_k64_, cee_u256_, map_amp_, map_size_, map_eq_)

#define MAP_KEYS_DECL_(K, V, amp, size, eq) \
    K * \
    map_keys_##K##_##V##_(map_(K, V) *m) { \
        K *keys = malloc(m->len * sizeof(*keys)); \
        size_t i = 0; \
        for (size_t j = 0; j < m->bktcap; j++) { \
            if (m->bkts[j].state == MAP_BKT_FULL_) { \
                cee_assert(i < m->len); \
                keys[i++] = m->bkts[j].key; \
            } \
        } \
        return keys; \
    }
MAP_ALL_IMPL_(MAP_KEYS_DECL_)

/* Everything past 29 is shamelessly stolen from "good hash table primes" by
 * akrowne on PlanetMath. */
static const size_t map_sizes_[] = {
    29, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317,
    196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843,
    50331653, 100663319, 201326611, 402653189, 805306457, 1610612741
};
const size_t MAP_SIZESLEN_ = sizeof(map_sizes_) / sizeof(size_t);

#define MAP_RESIZE_DECL_(K, V, amp, size, eq_) \
    static void \
    map_resize_##K##_##V##_(map_(K, V) *m, size_t sizeidx) { \
        cee_assert(sizeidx < MAP_SIZESLEN_); \
        size_t bktcap = map_sizes_[sizeidx]; \
        map_bkt_(K, V) *bkts = calloc(bktcap, sizeof(*bkts)); \
        for (size_t i = 0; i < m->bktcap; i++) { \
            if (m->bkts[i].state == MAP_BKT_FULL_) { \
                uint32_t hash = map_hash_( \
                    (unsigned char *)amp(m->bkts[i].key), \
                    size(m->bkts[i].key)); \
                for (size_t j = hash % bktcap; ; j = (j + 1) % bktcap) { \
                    if (bkts[j].state == MAP_BKT_EMPTY_) { \
                        bkts[j] = (map_bkt_(K, V)){ \
                            m->bkts[i].key, \
                            m->bkts[i].val, \
                            MAP_BKT_FULL_, \
                        }; \
                        break; \
                    } \
                } \
            } \
        } \
        free(m->bkts); \
        *m = (map_(K, V)){m->len, bkts, m->len, bktcap, sizeidx}; \
    }
MAP_ALL_IMPL_(MAP_RESIZE_DECL_)

/* Linear probing because I'm lazy. Replace with robin hood hashing or w/e. */
#define MAP_PUT_DECL_(K, V, amp, size, eq) \
    bool \
    map_put_##K##_##V##_(map_(K, V) *m, K key, V val, V *old_val) { \
        if ((double)m->bktlen / m->bktcap > 0.8) {\
            map_resize_##K##_##V##_(m, m->sizeidx + 1); \
        } \
        uint32_t hash = map_hash_((unsigned char *)amp(key), size(key)); \
        for (size_t i = hash % m->bktcap; ; i = (i + 1) % m->bktcap) { \
            switch (m->bkts[i].state) { \
            case MAP_BKT_EMPTY_: \
                m->bkts[i] = (map_bkt_(K, V)){key, val, MAP_BKT_FULL_}; \
                m->len++; \
                m->bktlen++; \
                return false; \
            case MAP_BKT_FULL_: \
                if (eq(key, m->bkts[i].key)) { \
                    if (old_val) { \
                        *old_val = m->bkts[i].val; \
                    } \
                    m->bkts[i] = (map_bkt_(K, V)){key, val, MAP_BKT_FULL_}; \
                    m->bktlen++; \
                    return true; \
                } \
                break; \
            case MAP_BKT_TOMB_: break; \
            } \
        } \
    }
MAP_ALL_IMPL_(MAP_PUT_DECL_)

#define MAP_GET_DECL_(K, V, amp, size, eq) \
    bool \
    map_get_##K##_##V##_(map_(K, V) *m, K key, V *val) { \
        uint32_t hash = map_hash_((unsigned char *)amp(key), size(key)); \
        for (size_t i = hash % m->bktcap; ; i = (i + 1) % m->bktcap) { \
            switch (m->bkts[i].state) { \
            case MAP_BKT_EMPTY_: \
                return false; \
            case MAP_BKT_FULL_: \
                if (eq(key, m->bkts[i].key)) { \
                    if (val) { \
                        *val = m->bkts[i].val; \
                    } \
                    return true; \
                } \
                break; \
            case MAP_BKT_TOMB_: break; \
            } \
        } \
    }
MAP_ALL_IMPL_(MAP_GET_DECL_)

#define MAP_DEL_DECL_(K, V, amp, size, eq) \
    bool \
    map_del_##K##_##V##_(map_(K, V) *m, K key, V *old_val) { \
        uint32_t hash = map_hash_((unsigned char *)amp(key), size(key)); \
        for (size_t i = hash % m->bktcap; ; i = (i + 1) % m->bktcap) { \
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
                    return true; \
                } \
                break; \
            case MAP_BKT_TOMB_: break; \
            } \
        } \
        return true; \
    }
MAP_ALL_IMPL_(MAP_DEL_DECL_)
