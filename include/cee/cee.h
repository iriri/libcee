/* Copyright 2018 iriri. All rights reserved. Use of this source code is
 * governed by the BSD-style license found in the LICENSE file. */
#ifndef CEE_CEE_H
#define CEE_CEE_H
#include <stdint.h>
#include <string.h>

#define p(T) p_paste_(T)
#define P_DEF(T) typedef T *p(T)

/* `cee_assert` never becomes a noop, even when `NDEBUG` is set. */
#define cee_assert(pred) \
    (__builtin_expect(!(pred), 0) ? \
        cee_assert_(__FILE__, __LINE__, #pred) : (void)0)

/* ---------------------------- Implementation ---------------------------- */
#define p_paste_(T) p_##T##_

__attribute__((cold, noreturn)) void cee_assert_(
    const char *, unsigned, const char *);

typedef uint32_t cee_u32_;
typedef uint64_t cee_u64_;

#define CEE_U_DECL_(b) \
    typedef struct cee_u##b##_ { \
        uint64_t p[b / 64]; \
    } cee_u##b##_;
CEE_U_DECL_(128)
CEE_U_DECL_(192)
CEE_U_DECL_(256)

#define cee_sym_(sym, id) CEE_##sym##id##_

/* Unfortunately you can't just cast anything into anything else without
 * violating the common interpretation of the "strict aliasing" rule. Should
 * (hopefully...) compile to a noop when optimization is enabled. */
#define cee_to_(T, v, id) __extension__ ({ \
    __auto_type cee_sym_(v_, id) = v; \
    T u; \
    memcpy(&u, &cee_sym_(v_, id), sizeof(u)); \
    u; \
})

#if defined(__i386__) || defined(__x86_64__)
#define cee_pause() __asm__ __volatile__("pause")
#elif defined(__arm__) || defined(__aarch64__)
#define cee_pause() __asm__ __volatile__("yield")
#else
#define cee_pause() __asm__ __volatile__("");
#endif

/* GCC collapses all of the pauses into a single instruction when looping past
 * 8(?) so... */
#define cee_pause16() if (1) { \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
    cee_pause(); \
} else (void)0
#endif
