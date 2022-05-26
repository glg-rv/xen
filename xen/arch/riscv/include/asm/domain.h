#ifndef __ASM_DOMAIN_H__
#define __ASM_DOMAIN_H__

#ifndef __ASSEMBLY__

#include <xen/cache.h>
#include <xen/sched.h>
#include <asm/page.h>
#include <asm/p2m.h>
#include <asm/vplic.h>
#include <public/hvm/params.h>
#include <xen/serial.h>
#include <xen/rbtree.h>


struct hvm_domain
{
    uint64_t              params[HVM_NR_PARAMS];
};

#ifdef CONFIG_RISCV_64
enum domain_type {
    DOMAIN_32BIT,
    DOMAIN_64BIT,
};
#define is_32bit_domain(d) ((d)->arch.type == DOMAIN_32BIT)
#define is_64bit_domain(d) ((d)->arch.type == DOMAIN_64BIT)
#else
#define is_32bit_domain(d) (1)
#define is_64bit_domain(d) (0)
#endif

/* The hardware domain has always its memory direct mapped. */
#define is_domain_direct_mapped(d) ((d) == hardware_domain)

struct vtimer {
        struct vcpu *v;
        struct timer timer;
};

struct arch_domain
{
    /* Virtual MMU */
    struct p2m_domain p2m;

    struct hvm_domain hvm;

}  __cacheline_aligned;

struct arch_vcpu
{
    /* Callee-saved registers and tp, gp, ra */
    struct
    {
        register_t s0;
        register_t s1;
        register_t s2;
        register_t s3;
        register_t s4;
        register_t s5;
        register_t s6;
        register_t s7;
        register_t s8;
        register_t s9;
        register_t s10;
        register_t s11;

        register_t sp;
        register_t gp;

        /* ra is used to jump to guest when creating new vcpu */
        register_t ra;
    } saved_context;

    struct cpu_info *cpu_info;
    void *stack;
    struct vplic *vplic;
    struct vtimer vtimer;
    bool vtimer_initialized;

    /* CSRs */
    register_t hstatus;
    register_t hedeleg;
    register_t hideleg;
    register_t hvip;
    register_t hip;
    register_t hie;
    register_t hgeie;
    register_t henvcfg;
    register_t hcounteren;
    register_t htimedelta;
    register_t htval;
    register_t htinst;
    register_t hgatp;
#ifdef CONFIG_32BIT
    register_t henvcfgh;
    register_t htimedeltah;
#endif
    /* VCSRs */
    register_t vsstatus;
    register_t vsip;
    register_t vsie;
    register_t vstvec;
    register_t vsscratch;
    register_t vscause;
    register_t vstval;
    register_t vsatp;

}  __cacheline_aligned;

struct arch_vcpu_io {
    /* TODO */
};

void vcpu_show_execution_state(struct vcpu *);
void vcpu_show_registers(const struct vcpu *);

static inline struct vcpu_guest_context *alloc_vcpu_guest_context(void)
{
    return xmalloc(struct vcpu_guest_context);
}

static inline void free_vcpu_guest_context(struct vcpu_guest_context *vgc)
{
    xfree(vgc);
}

static inline void arch_vcpu_block(struct vcpu *v) {}

#endif /* !__ASSEMBLY__ */

#define VCPU_SAVED_CONTEXT_s0               0
#define VCPU_SAVED_CONTEXT_s1               1
#define VCPU_SAVED_CONTEXT_s2               2
#define VCPU_SAVED_CONTEXT_s3               3
#define VCPU_SAVED_CONTEXT_s4               4
#define VCPU_SAVED_CONTEXT_s5               5
#define VCPU_SAVED_CONTEXT_s6               6
#define VCPU_SAVED_CONTEXT_s7               7
#define VCPU_SAVED_CONTEXT_s8               8
#define VCPU_SAVED_CONTEXT_s9               9
#define VCPU_SAVED_CONTEXT_s10              10
#define VCPU_SAVED_CONTEXT_s11              11
#define VCPU_SAVED_CONTEXT_sp               12
#define VCPU_SAVED_CONTEXT_gp               13
#define VCPU_SAVED_CONTEXT_ra               14
#define VCPU_SAVED_CONTEXT_last             15
#define VCPU_SAVED_CONTEXT_OFFSET(x)	\
    (VCPU_arch_saved_context + ((VCPU_SAVED_CONTEXT_##x) * __SIZEOF_POINTER__))
#define VCPU_SAVED_CONTEXT_SIZE		    VCPU_SAVED_CONTEXT_OFFSET(last)

#endif /* __ASM_DOMAIN_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
