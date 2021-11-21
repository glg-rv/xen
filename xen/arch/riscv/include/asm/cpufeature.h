#ifndef __ASM_RISCV_CPUFEATURE_H
#define __ASM_RISCV_CPUFEATURE_H

#define RISCV_NCAPS 1

#ifndef __ASSEMBLY__

#include <xen/types.h>
#include <xen/lib.h>
#include <xen/bitops.h>

extern DECLARE_BITMAP(cpu_hwcaps, RISCV_NCAPS);

static inline bool cpus_have_cap(unsigned int num)
{
    return false;
}

static inline int cpu_nr_siblings(unsigned int cpu)
{
    return 1;
}

static inline void cpus_set_cap(unsigned int num)
{
    if (num >= RISCV_NCAPS)
        printk(XENLOG_WARNING "Attempt to set an illegal CPU capability (%d >= %d)\n",
               num, RISCV_NCAPS);
    else
        __set_bit(num, cpu_hwcaps);
}

struct riscv_cpu_capabilities {
};

void update_cpu_capabilities(const struct riscv_cpu_capabilities *caps,
                             const char *info);

void enable_cpu_capabilities(const struct riscv_cpu_capabilities *caps);
int enable_nonboot_cpu_caps(const struct riscv_cpu_capabilities *caps);

/*
 * capabilities of CPUs
 */
struct cpuinfo_riscv {
};

extern struct cpuinfo_riscv boot_cpu_data;

extern void identify_cpu(struct cpuinfo_riscv *);

extern struct cpuinfo_riscv cpu_data[];
#define current_cpu_data cpu_data[smp_processor_id()]

extern struct cpuinfo_riscv guest_cpuinfo;

#endif /* __ASSEMBLY__ */

#endif
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

