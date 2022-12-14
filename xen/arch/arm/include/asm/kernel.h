/*
 * Kernel image loading.
 *
 * Copyright (C) 2011 Citrix Systems, Inc.
 */
#ifndef __ARCH_ARM_KERNEL_H__
#define __ARCH_ARM_KERNEL_H__

#include <xen/device_tree.h>
#include <asm/setup.h>

struct kernel_info {
#ifdef CONFIG_ARM_64
    enum domain_type type;
#endif

    struct domain *d;

    void *fdt; /* flat device tree */
    paddr_t unassigned_mem; /* RAM not (yet) assigned to a bank */
    struct meminfo mem;

    /* kernel entry point */
    paddr_t entry;

    /* grant table region */
    paddr_t gnttab_start;
    paddr_t gnttab_size;

    /* boot blob load addresses */
    const struct bootmodule *kernel_bootmodule, *initrd_bootmodule, *dtb_bootmodule;
    const char* cmdline;
    paddr_t dtb_paddr;
    paddr_t initrd_paddr;

    /* Enable pl011 emulation */
    bool vpl011;

    /* Enable PV drivers */
    bool dom0less_enhanced;

    /* GIC phandle */
    uint32_t phandle_gic;

    /* loader to use for this kernel */
    void (*load)(struct kernel_info *info);
    /* loader specific state */
    union {
        struct {
            paddr_t kernel_addr;
            paddr_t len;
#ifdef CONFIG_ARM_64
            paddr_t text_offset; /* 64-bit Image only */
#endif
            paddr_t start; /* 32-bit zImage only */
        } zimage;
    };
};

/*
 * Probe the kernel to detemine its type and select a loader.
 *
 * Sets in info:
 *  ->type
 *  ->load hook, and sets loader specific variables ->zimage
 */
int kernel_probe(struct kernel_info *info, const struct dt_device_node *domain);

/*
 * Loads the kernel into guest RAM.
 *
 * Expects to be set in info when called:
 *  ->mem
 *  ->fdt
 *
 * Sets in info:
 *  ->entry
 *  ->dtb_paddr
 *  ->initrd_paddr
 */
void kernel_load(struct kernel_info *info);

#endif /* #ifdef __ARCH_ARM_KERNEL_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
