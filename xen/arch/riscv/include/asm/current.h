#ifndef __ASM_CURRENT_H
#define __ASM_CURRENT_H

#include <xen/percpu.h>
#include <asm/processor.h>

struct vcpu;

/* Which VCPU is "current" on this PCPU. */
DECLARE_PER_CPU(struct vcpu *, curr_vcpu);

#define current            (this_cpu(curr_vcpu))
#define set_current(vcpu)  do { current = (vcpu); } while (0)
#define get_cpu_current(cpu)  (per_cpu(curr_vcpu, cpu))

/* Per-VCPU state that lives at the top of the stack */
struct cpu_info {
    struct cpu_user_regs guest_cpu_user_regs;
    unsigned long elr;
    uint32_t flags;
};

static inline struct cpu_info *get_cpu_info(void)
{
    register unsigned long sp asm ("sp");
    return (struct cpu_info *)((sp & ~(STACK_SIZE - 1)) + STACK_SIZE - sizeof(struct cpu_info));
}

#define guest_cpu_user_regs() (&get_cpu_info()->guest_cpu_user_regs)
#define guest_regs(vcpu) (&vcpu->arch.cpu_info->guest_cpu_user_regs)

#define switch_stack_and_jump(stack, fn)                                \
    asm volatile (                                                      \
            "mv sp, %0 \n"                                              \
            "j " #fn :: "r" (stack) :                                   \
    )

#define reset_stack_and_jump(fn) switch_stack_and_jump(get_cpu_info(), fn)

DECLARE_PER_CPU(unsigned int, cpu_id);

#endif /* __ASM_CURRENT_H */
