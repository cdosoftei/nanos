#pragma once

#include <runtime.h>
#include <vdso_decls.h>
#include <pvclock.h>

#define do_syscall(sysnr, rdi, rsi) ({\
    sysreturn rv;\
    asm("syscall"\
        : "=a" (rv)\
        : "0" (sysnr), "D" (rdi), "S"(rsi)\
        : "memory"\
    );\
    rv;\
})

/* An instance of this struct is shared between kernel and userspace
 * Make sure there are no pointers embedded in it
 */
struct vdso_dat_struct {
    vdso_clock_id clock_src;
    timestamp rtc_offset;
    u8 platform_has_rdtscp;
} __attribute((packed));

/* VDSO accessible variables */
VVAR_DECL(struct vdso_dat_struct, vdso_dat);

/* now routines that are accessible from both the VDSO and the core kernel */
VDSO u64 vdso_pvclock_now_ns(volatile struct pvclock_vcpu_time_info *);
