/* VDSO syscall implementations */

#include <runtime.h>
#include <unix_internal.h>
#include <vdso.h>

#define __vdso_dat (&(VVAR_REF(vdso_dat)))

extern VVAR void * pvclock_page;
#define vclock ((volatile struct pvclock_vcpu_time_info *)&pvclock_page)

static sysreturn
fallback_clock_gettime(clockid_t clk_id, struct timespec * tp)
{
    return do_syscall(SYS_clock_gettime, clk_id, tp);
}

static sysreturn 
fallback_gettimeofday(struct timeval * tv, void * tz)
{
    return do_syscall(SYS_gettimeofday, tv, tz);
}

static sysreturn
fallback_time(time_t * t)
{
    return do_syscall(SYS_time, t, 0);
}

static sysreturn
do_vdso_clock_gettime_pvclock(clockid_t clk_id, struct timespec * tp)
{
    timestamp ts;

    switch (clk_id) {
    case CLOCK_ID_MONOTONIC:
    case CLOCK_ID_MONOTONIC_RAW:
    case CLOCK_ID_MONOTONIC_COARSE:
    case CLOCK_ID_BOOTTIME:
        ts = nanoseconds(vdso_pvclock_now_ns(vclock));

    case CLOCK_ID_REALTIME:
    case CLOCK_ID_REALTIME_COARSE:
        ts = nanoseconds(vdso_pvclock_now_ns(vclock)) + __vdso_dat->rtc_offset;

    default:
        return fallback_clock_gettime(clk_id, tp);
    }

    timespec_from_time(tp, ts);
    return 0;
}

static sysreturn
do_vdso_clock_gettime(clockid_t clk_id, struct timespec * tp)
{
    switch (__vdso_dat->clock_src) {
    case VDSO_CLOCK_PVCLOCK:
        return do_vdso_clock_gettime_pvclock(clk_id, tp);

    case VDSO_CLOCK_SYSCALL:
    default:
        return fallback_clock_gettime(clk_id, tp);
    }
}

static sysreturn
do_vdso_gettimeofday(struct timeval * tv, void * tz)
{
    switch (__vdso_dat->clock_src) {
    case VDSO_CLOCK_PVCLOCK:
        timeval_from_time(tv, nanoseconds(vdso_pvclock_now_ns(vclock))
            + __vdso_dat->rtc_offset);
        return 0;

    case VDSO_CLOCK_SYSCALL:
    default:
        return fallback_gettimeofday(tv, tz);
    }
}

static sysreturn
do_vdso_getcpu(unsigned * cpu, unsigned * node, void * tcache)
{
    if (cpu)
        *cpu = 0;
    if (node)
        *node = 0;
    return 0;
}

static sysreturn
do_vdso_time(time_t * t)
{
    time_t ret;

    switch (__vdso_dat->clock_src) {
    case VDSO_CLOCK_PVCLOCK:
        ret = time_t_from_time(nanoseconds(vdso_pvclock_now_ns(vclock)));
        if (t)
            *t = ret;
        return ret;

    case VDSO_CLOCK_SYSCALL:
    default:
        return fallback_time(t);
    }
}




/* --------------------------------------------------------------------- */
/* Below are the full set of visible functions exported through the VDSO */
/*              Everything above must be marked static                   */
/* --------------------------------------------------------------------- */

sysreturn 
__vdso_clock_gettime(clockid_t clk_id, struct timespec * tp)
{
    return do_vdso_clock_gettime(clk_id, tp);
}

sysreturn
clock_gettime(clockid_t clk_id, struct timespec * tp)
{
    return do_vdso_clock_gettime(clk_id, tp);
}

sysreturn 
__vdso_gettimeofday(struct timeval * tv, void * tz)
{
    return do_vdso_gettimeofday(tv, tz);
}

sysreturn
gettimeofday(struct timeval * tv, void * tz)
{
    return do_vdso_gettimeofday(tv, tz);
}

sysreturn
__vdso_getcpu(unsigned * cpu, unsigned * node, void * tcache)
{
    return do_vdso_getcpu(cpu, node, tcache);
}

sysreturn
getcpu(unsigned * cpu, unsigned * node, void * tcache)
{
    return do_vdso_getcpu(cpu, node, tcache);
}

sysreturn
__vdso_time(time_t * t)
{
    return do_vdso_time(t);
}

sysreturn
time(time_t * t)
{
    return do_vdso_time(t);
}
