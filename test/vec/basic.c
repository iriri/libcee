#include <assert.h>
#include <stdio.h>

#include <cee/vec.h>

typedef void (*fn)(void);
VEC_DEF(int);
VEC_DEF(fn);

void
verify(vec(int) v, const int ia[], size_t len) {
    if (vec_len(v) != len) {
        printf("len mismatch: %lu, %lu\n", vec_len(v), len);
        assert(0);
    }
    for (size_t i = 0; i < vec_len(v); i++) {
        if (vec_arr(v)[i] != ia[i]) {
            printf("%zu, %d, %d\n", i, vec_arr(v)[i], ia[i]);
            assert(0);
        }
    }
}

void
foo(void) {
    static int a = 0;
    printf("%d\n", ++a);
}

int
main(void) {
    vec(int) v = vec_make(int, 0, 2);
    int a = 1;
    vec_push(&v, a);
    vec_push(&v, 2);
    assert(vec_pop(&v, &a) && a == 2);
    vec_push(&v, 2);
    vec_push(&v, 3);
    assert(vec_cap(v) == 4);
    vec_push(&v, 4);
    vec_push(&v, 5);
    assert(vec_cap(v) == 8);
    int via[] = {1, 2, 3, 4, 5};
    verify(v, via, 5);
    assert(vec_peek(v, &a));
    assert(a == 5);

    vec_remove(&v, 4);
    int via1[] = {1, 2, 3, 4};
    verify(v, via1, 4);
    vec_remove(&v, 2);
    int via2[] = {1, 2, 4};
    verify(v, via2, 3);
    vec_remove(&v, vec_find(v, 1));
    vec_shrink(&v);
    assert(vec_cap(v) == 4);
    vec_trim(&v);
    assert(vec_cap(v) == 2);
    int via3[] = {2, 4};
    verify(v, via3, 2);
    vec_idx(v, 0) = 1;
    a = vec_idx(v, 0);
    assert(a == 1);

    vec(int) v1 = vec_make(int, 5, 6);
    for (size_t i = 0; i < vec_len(v1); i++) {
        vec_arr(v1)[i] = i + 1;
    }
    verify(v1, via, 5);
    vec_concat(&v, v1);
    int via4[] = {1, 4, 1, 2, 3, 4, 5};
    verify(v, via4, 7);
    v = vec_drop(v);
    v1 = vec_drop(v1);

    vec(fn) v2 = vec_make(fn, 0, 2);
    vec_push(&v2, &foo);
    vec_push(&v2, &foo);
    for (size_t i = 0; i < vec_len(v2); i++) {
        vec_arr(v2)[i]();
    }
    v2 = vec_drop(v2);

    vec(int) v3 = vec_make(int, 0, 2);
    vec_trim(&v3);
    vec_push(&v3, 1);
    vec_push(&v3, 2);
    verify(v3, (const int[]){1, 2}, 2);
    v3 = vec_drop(v3);
    printf("All tests passed\n");
    return 0;
}
