#ifndef __ASM_VPLIC_H__
#define __ASM_VPLIC_H__

#include <asm/processor.h>
#include <xen/sched.h>

#define MAX_SOURCES 1024
#define MAX_CONTEXTS 15872

struct context {
    u32 enable[MAX_SOURCES/32];
};

struct vplic {
    unsigned int num_contexts;
    struct context *contexts;
    unsigned long base;
};

int vplic_emulate_load(struct vcpu *vcpu, unsigned long addr, void *out, int out_len);
struct vplic *vplic_alloc(void);

#endif /* __ASM_VPLIC_H__ */
