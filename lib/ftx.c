#ifdef __linux__
#include "ftx+linux.i"
#elif __FreeBSD__
#error "soon"
#elif __OpenBSD__
#error "soon"
#elif __APPLE__
#include "ftx+apple.i"
#else
#error "probably never"
#endif
