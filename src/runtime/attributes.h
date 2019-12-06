/* attributes for compiling+linking */

#define NOTRACE __attribute__((no_instrument_function))
#define HIDDEN  __attribute__((visibility("hidden")))
#define VDSO     NOTRACE HIDDEN
#define VVAR     HIDDEN
#define VSYSCALL NOTRACE __attribute__((section(".vsyscall")))
