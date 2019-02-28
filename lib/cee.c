#include <stdio.h>
#include <stdlib.h>

#include <cee/cee.h>

__attribute__((noreturn)) void
cee_assert_(const char *file, unsigned line, const char *pred) {
    fprintf(stderr, "Failed assertion: %s, %u, %s\n", file, line, pred);
    abort();
}
