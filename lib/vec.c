#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cee/cee.h>

#include <cee/vec.h>

void *
vec_alloc_(size_t eltsz, size_t len, size_t cap) {
    cee_assert(eltsz <= SIZE_MAX / 8 && cap <= SIZE_MAX / eltsz && len <= cap);
    void *arr = malloc(cap * eltsz);
    cee_assert(arr || cap == 0);
    return arr;
}

void
vec_shrink_(vec_ *v, size_t eltsz) {
    if (v->len * 4 > v->cap) {
        return;
    }
    cee_assert(
        (v->arr = realloc(v->arr, (v->cap = 2 * v->len) * eltsz)) ||
        v->len == 0);
}

void
vec_trim_(vec_ *v, size_t eltsz) {
    if (v->len == v->cap) {
        return;
    }
    cee_assert(
        (v->arr = realloc(v->arr, (v->cap = v->len) * eltsz)) || v->len == 0);
}

void
vec_grow_(vec_ *v, size_t eltsz) {
    if (v->cap == 0) {
        v->cap = 8;
    } else {
        cee_assert(
            v->cap < SIZE_MAX - v->cap && (v->cap *= 2) <= SIZE_MAX / eltsz);
    }
    cee_assert((v->arr = realloc(v->arr, v->cap * eltsz)));
}

void
vec_concat_(vec_ *v, vec_ v1, size_t eltsz) {
    if (v->len + v1.len > v->cap) {
        size_t g = v->cap > v1.cap ? v->cap : v1.cap;
        cee_assert(v->cap < SIZE_MAX - g && (v->cap += g) <= SIZE_MAX / eltsz);
        cee_assert((v->arr = realloc(v->arr, v->cap * eltsz)));
    }
    memmove(v->arr + (v->len * eltsz), v1.arr, v1.len * eltsz);
    v->len += v1.len;
}

void
vec_remove_(vec_ *v, size_t index, size_t eltsz) {
    if (index == SIZE_MAX) {
        return;
    }
    v->len--;
    cee_assert(index <= v->len);
    if (index < v->len) {
        memmove(
            v->arr + (index * eltsz),
            v->arr + ((index + 1) * eltsz),
            (v->len - index) * eltsz);
    }
}
