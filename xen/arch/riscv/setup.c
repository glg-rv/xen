/*
 * xen/arch/riscv/setup.c
 *
 *
 * Early bringup code for a RISC-V RV32/64 with hypervisor
 * extensions (code H).
 *
 * Based off the ARM setup code with copyright Tim Deegan <tim@xen.org>
 *
 * Copyright (c) 2019 Bobby Eshleman <bobbyeshleman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <asm/guest_access.h>
#include <asm/domain_build.h>
#include <asm/domain.h>
#include <xen/compile.h>
#include <xen/domain_page.h>
#include <xen/grant_table.h>
#include <xen/types.h>
#include <xen/string.h>
#include <xen/serial.h>
#include <xen/sched.h>
#include <xen/console.h>
#include <xen/err.h>
#include <xen/init.h>
#include <xen/irq.h>
#include <xen/mm.h>
#include <xen/softirq.h>
#include <xen/time.h>
#include <xen/keyhandler.h>
#include <xen/cpu.h>
#include <xen/pfn.h>
#include <xen/virtual_region.h>
#include <xen/vmap.h>
#include <xen/trace.h>
#include <asm/page.h>
#include <asm/current.h>
#include <asm/setup.h>
#include <asm/traps.h>
#include <xsm/xsm.h>

static void setup_trap_handler(void)
{
    unsigned long addr = (unsigned long)&handle_exception;
    csr_write(CSR_STVEC, addr);
    printk("CSR_STVEC=0x%02lx\n", csr_read(CSR_STVEC));
}

void idle_loop(void)
{
    unsigned int cpu = smp_processor_id();

    printk("%s\n", __func__);

    for ( ; ; )
    {
        if ( unlikely(tasklet_work_to_do(cpu)) )
            do_tasklet();
        do_softirq();
    }
}

void startup_cpu_idle_loop(void)
{
    struct vcpu *v = current;

    printk("%s\n", __func__);

    ASSERT(is_idle_vcpu(v));

    reset_stack_and_jump(idle_loop);

    /* This function is noreturn */
    while (1);
}

static __used void init_done(void)
{
    /* TODO: free init memory */
    startup_cpu_idle_loop();
}

static unsigned int __initdata max_cpus = NR_CPUS;

struct bootinfo __initdata bootinfo;

struct domain *xen_dom;

/* The lucky hart to first increment this variable will boot the other cores */
atomic_t hart_lottery;
unsigned long boot_cpu_hartid;
unsigned long total_pages;

/*
 * boot_cmdline_find_by_kind can only be used to return Xen modules (e.g
 * XSM, DTB) or Dom0 modules. This is not suitable for looking up guest
 * modules.
 */
struct bootcmdline * __init boot_cmdline_find_by_kind(bootmodule_kind kind)
{
    struct bootcmdlines *cmds = &bootinfo.cmdlines;
    struct bootcmdline *cmd;
    int i;

    for ( i = 0 ; i < cmds->nr_mods ; i++ )
    {
        cmd = &cmds->cmdline[i];
        if ( cmd->kind == kind && !cmd->domU )
            return cmd;
    }
    return NULL;
}

struct bootcmdline * __init boot_cmdline_find_by_name(const char *name)
{
    struct bootcmdlines *mods = &bootinfo.cmdlines;
    struct bootcmdline *mod;
    unsigned int i;

    for (i = 0 ; i < mods->nr_mods ; i++ )
    {
        mod = &mods->cmdline[i];
        if ( strcmp(mod->dt_name, name) == 0 )
            return mod;
    }
    return NULL;
}

struct bootmodule * __init boot_module_find_by_addr_and_kind(bootmodule_kind kind,
                                                             paddr_t start)
{
    struct bootmodules *mods = &bootinfo.modules;
    struct bootmodule *mod;
    unsigned int i;

    for (i = 0 ; i < mods->nr_mods ; i++ )
    {
        mod = &mods->module[i];
        if ( mod->kind == kind && mod->start == start )
            return mod;
    }
    return NULL;
}

struct bootmodule kernel_bm = {
    .kind = BOOTMOD_KERNEL,
    .domU = false,
    .start = DOM0_KERNEL,
    .size = DOM0_KERNEL_SIZE,
};

/*
 * boot_module_find_by_kind can only be used to return Xen modules (e.g
 * XSM, DTB) or Dom0 modules. This is not suitable for looking up guest
 * modules.
 */
struct bootmodule * __init boot_module_find_by_kind(bootmodule_kind kind)
{
    struct bootmodules *mods = &bootinfo.modules;
    struct bootmodule *mod;
    int i;

    if (kind == BOOTMOD_KERNEL)
        return &kernel_bm;

    for (i = 0 ; i < mods->nr_mods ; i++ )
    {
        mod = &mods->module[i];
        if ( mod->kind == kind && !mod->domU )
            return mod;
    }
    return NULL;
}

void arch_get_xen_caps(xen_capabilities_info_t *info)
{
    /* Interface name is always xen-3.0-* for Xen-3.x. */
    int major = 3, minor = 0;
    char s[32];

    (*info)[0] = '\0';

    snprintf(s, sizeof(s), "xen-%d.%d-riscv ", major, minor);
    safe_strcat(*info, s);
}

/*
 * TODO: Do not hardcode this.  There has been discussion on how OpenSBI will
 * communicate it's protected space to its payload.  Xen will need to conform
 * to that approach.
 *
 * 0x80000000 - 0x80200000 is PMP protected by OpenSBI so exclude it from the
 * ram range (any attempt at using it will trigger a PMP fault).
 */
#define OPENSBI_OFFSET 0x0200000
#define XEN_OFFSET (2 << 20)

static void __init setup_mm(mfn_t dom0_kern_start, mfn_t dom0_kern_end)
{
    paddr_t ram_start, ram_end, ram_size;

    /* TODO: Use FDT instead of hardcoding these values */

    /*
     * For now, just skip over the boot modules (DTB is the last one.
     * see DOM0_KERNEL/DTB declarations for layout of boot omdules)
     */
    ram_start = PAGE_ALIGN(DTB + DTB_SIZE);
    ram_end   = 0xc0000000;
    ram_size = ram_end - ram_start;

    total_pages = ram_size >> PAGE_SHIFT;
    pfn_pdx_hole_setup(0);
    setup_xenheap_mappings(ram_start>>PAGE_SHIFT, total_pages);
    xenheap_virt_end = XENHEAP_VIRT_START + ram_size;
    xenheap_mfn_end = maddr_to_mfn(ram_end);
    init_boot_pages(mfn_to_maddr(xenheap_mfn_start),
                    mfn_to_maddr(xenheap_mfn_end));
    max_page = PFN_DOWN(ram_end);
    setup_frametable_mappings(0, ram_end);
    setup_fixmap_mappings();
}

/** start_xen - The C entry point
 *
 * The real entry point is in head.S.
 */
void __init start_xen(paddr_t fdt_start)
{
    struct ns16550_defaults ns16550 = {
        .data_bits = 8,
        .parity    = 'n',
        .stop_bits = 1
    };
    struct domain *dom0;
    struct xen_domctl_createdomain dom0_cfg = {
        .flags = XEN_DOMCTL_CDF_hvm | XEN_DOMCTL_CDF_hap,
        .max_evtchn_port = -1,
        .max_grant_frames = gnttab_dom0_frames(),
        .max_maptrack_frames = -1,
    };
    unsigned int i;

    nr_cpu_ids = NR_CPUS;

    setup_virtual_regions(NULL, NULL);
    smp_clear_cpu_maps();

    init_xen_time();

    setup_mm(_mfn(DOM0_KERNEL), _mfn(DOM0_KERNEL + DOM0_KERNEL_SIZE));
    end_boot_allocator();

    /*
     * system_state is SYS_STATE_boot after the boot allocator has ended and the
     * memory subsystem has been initialized.
     */
    system_state = SYS_STATE_boot;

    percpu_init_areas();

    vm_init();

    softirq_init();

    tasklet_subsys_init();

    do_presmp_initcalls();

    for_each_present_cpu ( i )
    {
        if ( (num_online_cpus() < max_cpus) && !cpu_online(i) )
        {
            int ret = cpu_up(i);
            if ( ret != 0 )
                printk("Failed to bring up CPU %u (error %d)\n", i, ret);
        }
    }

    ns16550.io_base = 0x10000000;
    ns16550.irq     = 10;
    ns16550.baud    = 115200;
    ns16550_init(0, &ns16550);
    console_init_preirq();

    printk("RISC-V Xen Boot!\n");

    timer_init();

    rcu_init();

    setup_system_domains();

    setup_trap_handler();

    local_irq_enable();

    /* Init idle domain */
    scheduler_init();
    set_current(idle_vcpu[0]);

    do_initcalls();

    dom0_cfg.max_vcpus = NR_VCPUS;
    dom0 = domain_create(0, &dom0_cfg, true);
    if ( IS_ERR(dom0) || (alloc_dom0_vcpu0(dom0) == NULL) )
        panic("Error creating domain 0\n");

    if ( construct_dom0(dom0) != 0)
        panic("Could not set up DOM0 guest OS\n");

    domain_unpause_by_systemcontroller(dom0);
    memcpy(idle_vcpu[0]->arch.cpu_info, get_cpu_info(),
           sizeof(struct cpu_info));
    switch_stack_and_jump(idle_vcpu[0]->arch.cpu_info, init_done);

    printk("end of start_xen\n");
}
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
