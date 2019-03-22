#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cee/cee.h>

#include <cee/vec.h>

void *
vec_alloc_(size_t eltsize, size_t len, size_t cap) {
    cee_assert(
        eltsize <= SIZE_MAX / 8 && // Ugh lol; see `vec_grow_`
        cap <= SIZE_MAX / eltsize &&
        len <= cap);
    void *arr = malloc(cap * eltsize);
    cee_assert(arr || cap == 0);
    return arr;
}

void
vec_shrink_(vec_ *v, size_t eltsize) {
    if (v->len * 4 > v->cap) {
        return;
    }
    cee_assert(
        (v->arr = realloc(v->arr, (v->cap = 2 * v->len) * eltsize)) ||
        v->len == 0);
}

void
vec_trim_(vec_ *v, size_t eltsize) {
    if (v->len == v->cap) {
        return;
    }
    cee_assert(
        (v->arr = realloc(v->arr, (v->cap = v->len) * eltsize)) ||
        v->len == 0);
}

void
vec_grow_(vec_ *v, size_t eltsize) {
    if (v->cap == 0) {
        v->cap = 8;
    } else {
        cee_assert(
            v->cap < SIZE_MAX - v->cap && (v->cap *= 2) <= SIZE_MAX / eltsize);
    }
    cee_assert((v->arr = realloc(v->arr, v->cap * eltsize)));
}

void
vec_concat_(vec_ *v, vec_ v1, size_t eltsize) {
    if (v->len + v1.len > v->cap) {
        size_t g = v->cap > v1.cap ? v->cap : v1.cap;
        cee_assert(
            v->cap < SIZE_MAX - g && (v->cap += g) <= SIZE_MAX / eltsize);
        cee_assert((v->arr = realloc(v->arr, v->cap * eltsize)));
    }
    memmove(v->arr + (v->len * eltsize), v1.arr, v1.len * eltsize);
    v->len += v1.len;
}

void
vec_remove_(vec_ *v, size_t index, size_t eltsize) {
    if (index == SIZE_MAX) {
        return;
    }
    v->len--;
    cee_assert(index <= v->len);
    if (index < v->len) {
        memmove(
            v->arr + (index * eltsize),
            v->arr + ((index + 1) * eltsize),
            (v->len - index) * eltsize);
    }
}
