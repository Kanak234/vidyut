/* vidyut_boot.c - run before and after the user's main().
 * Part of VIDYUT. MIT licence.
 */
#include "vidyut_internal.h"
#include <stdlib.h>
#include <signal.h>

static void on_signal(int sig)
{
    vg_atexit();
    /* Restore the default action and re-raise, so the exit status is honest. */
    signal(sig, SIG_DFL);
    raise(sig);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
#endif
static void vidyut_boot(void)
{
    /* Over a pipe the C library would buffer stdout in 4K blocks, so the
     * program's own printf output would arrive long after the drawing it
     * belongs with. Unbuffered keeps the two in step. */
    setvbuf(stdout, NULL, _IONBF, 0);
    atexit(vg_atexit);
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGSEGV, on_signal);
#ifdef SIGABRT
    signal(SIGABRT, on_signal);
#endif
}

#if !defined(__GNUC__) && !defined(__clang__)
/* Fallback for toolchains without constructor support: the first graphics
 * call registers the hooks instead. */
void vidyut_manual_boot(void) { vidyut_boot(); }
#endif
