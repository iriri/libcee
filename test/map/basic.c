#include <assert.h>
#include <stdio.h>

#include <cee/cee.h>
#include <cee/map.h>

typedef struct pair {
    uint64_t a, b;
} pair;

MAP_DEF(int, int);
MAP_DEF(uint64_t, pair);

int
main(void) {
    map(int, int) *m = map_make(int, int);
    assert(map_put(m, 1, 1, NULL) == false);
    int a;
    assert(map_get(m, 1, &a) == true);
    assert(a == 1);
    assert(map_put(m, 1, 2, NULL) == true);
    assert(map_get(m, 1, &a) == true);
    assert(a == 2);
    assert(map_put(m, 1, 3, NULL) == true);
    assert(map_del(m, 1, &a) == true);
    assert(a == 3);
    assert(map_get(m, 1, NULL) == false);
    m = map_drop(m);

    map(uint64_t, pair) *m1 = map_make(uint64_t, pair);
    for (uint64_t i = 0; i < 10000; i++) {
        assert(map_put(m1, i, ((pair){i, i + 1}), NULL) == false);
    }
    pair b;
    for (uint64_t i = 0; i < 10000; i++) {
        assert(map_get(m1, i, &b) == true);
        assert(b.a == i && b.b == i + 1);
    }
    for (uint64_t i = 0; i < 5000; i++) {
        assert(map_del(m1, i, NULL) == true);
    }
    uint64_t *keys = map_keys(m1);
    for (size_t i = 0; i < map_len(m1); i++) {
        assert(map_get(m1, keys[i], &b) == true);
        assert(b.a == keys[i] && b.b == keys[i] + 1);
    }
    free(keys);
    m1 = map_drop(m1);
}
