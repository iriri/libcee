#include <assert.h>
#include <stdlib.h>

#include <cee/cee.h>
#include <cee/map.h>

typedef struct pair {
    uint64_t a, b;
} pair;

MAP_DEF(int, int);
MAP_DEF(uint64_t, pair);
MAP_DEF_PV(char, uint64_t);

int
main(void) {
    map(int, int) *m = map_make(int, int);
    assert(map_put(m, 1, 1) == false);
    int a;
    assert(map_get(m, 1, &a) == true);
    assert(a == 1);
    assert(map_rep(m, 1, 2, &a) == true);
    assert(a == 1);
    assert(map_get(m, 1, &a) == true);
    assert(a == 2);
    assert(map_put(m, 1, 3) == true);
    assert(map_rem(m, 1, &a) == true);
    assert(a == 3);
    assert(map_has(m, 1) == false);
    m = map_drop(m);

    map(uint64_t, pair) *m1 = map_make(uint64_t, pair);
    for (uint64_t i = 0; i < 10000; i++) {
        assert(map_put(m1, i, ((const pair){i, i + 1})) == false);
    }
    pair b;
    for (uint64_t i = 0; i < 10000; i++) {
        assert(map_get(m1, i, &b) == true);
        assert(b.a == i && b.b == i + 1);
    }
    for (uint64_t i = 0; i < 5000; i++) {
        assert(map_del(m1, i) == true);
    }
    uint64_t *keys = map_keys(m1);
    for (size_t i = 0; i < map_len(m1); i++) {
        assert(map_get(m1, keys[i], &b) == true);
        assert(b.a == keys[i] && b.b == keys[i] + 1);
    }
    free(keys);
    m1 = map_drop(m1);

    map(p(char), uint64_t) *m2 = map_make(p(char), uint64_t);
    struct {char *k; uint64_t v;} kvs[] = {
        {"abc", 123}, {"def", 456}, {"ghi", 789},
    };
    const size_t kvslen = sizeof(kvs) / sizeof(kvs[0]);
    for (size_t i = 0; i < kvslen; i++) {
        assert(map_put(m2, kvs[i].k, kvs[i].v) == false);
    }
    uint64_t c;
    assert(map_rep(m2, "abc", (uint64_t)987, &c) == true);
    assert(c == 123);
    for (size_t i = 1; i < kvslen; i++) {
        assert(map_get(m2, kvs[i].k, &c) == true);
        assert(c == kvs[i].v);
    }
    m2 = map_drop(m2);
}
