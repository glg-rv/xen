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
#include <xen/sort.h>
#include <asm/page.h>
#include <asm/current.h>
#include <asm/setup.h>
#include <asm/traps.h>
#include <xsm/xsm.h>
#include <asm/plic.h>

extern void uart_init(void);

static void setup_trap_handler(void)
{
    unsigned long addr = (unsigned long)&handle_exception;
    csr_write(CSR_STVEC, addr);
    printk("CSR_STVEC=0x%02lx\n", csr_read(CSR_STVEC));
}

static __used void init_done(void)
{
    /* TODO: free init memory */
    startup_cpu_idle_loop();
}

static unsigned int __initdata max_cpus = NR_CPUS;

struct bootinfo __initdata bootinfo;
domid_t max_init_domid = 0;

struct domain *xen_dom;

bool acpi_disabled = true;

/* The lucky hart to first increment this variable will boot the other cores */
atomic_t hart_lottery;
unsigned long boot_cpu_hartid;
unsigned long total_pages;

void arch_get_xen_caps(xen_capabilities_info_t *info)
{
    /* Interface name is always xen-3.0-* for Xen-3.x. */
    int major = 3, minor = 0;
    char s[32];

    (*info)[0] = '\0';

    snprintf(s, sizeof(s), "xen-%d.%d-riscv ", major, minor);
    safe_strcat(*info, s);
}


/* This function assumes that memory regions are not overlapped */
static int __init cmp_memory_node(const void *key, const void *elem)
{
    const struct membank *handler0 = key;
    const struct membank *handler1 = elem;

    if ( handler0->start < handler1->start )
        return -1;

    if ( handler0->start >= (handler1->start + handler1->size) )
        return 1;

    return 0;
}


static void __init init_pdx(void)
{
    paddr_t bank_start, bank_size, bank_end;

    /*
     * Arm does not have any restrictions on the bits to compress. Pass 0 to
     * let the common code further restrict the mask.
     *
     * If the logic changes in pfn_pdx_hole_setup we might have to
     * update this function too.
     */
    uint64_t mask = pdx_init_mask(0x0);
    int bank;

    for ( bank = 0 ; bank < bootinfo.mem.nr_banks; bank++ )
    {
        bank_start = bootinfo.mem.bank[bank].start;
        bank_size = bootinfo.mem.bank[bank].size;

        mask |= bank_start | pdx_region_mask(bank_start, bank_size);
    }

    for ( bank = 0 ; bank < bootinfo.mem.nr_banks; bank++ )
    {
        bank_start = bootinfo.mem.bank[bank].start;
        bank_size = bootinfo.mem.bank[bank].size;

        if (~mask & pdx_region_mask(bank_start, bank_size))
            mask = 0;
    }

    pfn_pdx_hole_setup(mask >> PAGE_SHIFT);

    for ( bank = 0 ; bank < bootinfo.mem.nr_banks; bank++ )
    {
        bank_start = bootinfo.mem.bank[bank].start;
        bank_size = bootinfo.mem.bank[bank].size;
        bank_end = bank_start + bank_size;

        set_pdx_range(paddr_to_pfn(bank_start),
                      paddr_to_pfn(bank_end));
    }
}

static void generic_swap(void *a, void *b, size_t size)
{
    char t;

    do {
        t = *(char *)a;
        *(char *)a++ = *(char *)b;
        *(char *)b++ = t;
    } while ( --size > 0 );
}


static void __init setup_memory_region(paddr_t bank_start, paddr_t bank_end)
{
    paddr_t bank_size = bank_end - bank_start;
    paddr_t s, e;

    /* common/bootfdt.c */
    paddr_t __init next_module(paddr_t s, paddr_t *end);
    void __init fw_unreserved_regions(paddr_t s, paddr_t e,
                                      void (*cb)(paddr_t, paddr_t), int first);

    setup_xenheap_mappings(bank_start>>PAGE_SHIFT, bank_size>>PAGE_SHIFT);

    s = bank_start;
    while ( s < bank_end )
    {
        paddr_t n = bank_end;

        e = next_module(s, &n);

        if ( e == ~(paddr_t)0 )
        {
            e = n = bank_end;
        }

        if ( e > bank_end )
            e = bank_end;

        fw_unreserved_regions(s, e, init_boot_pages, 0);
        s = n;
    }
}

static void __init setup_mm(void)
{
    paddr_t ram_start = ~0;
    paddr_t ram_end = 0;
    paddr_t ram_size = 0;
    int bank;

    /* Register reserved memory as boot modules. */
    for ( bank = 0; bank < bootinfo.reserved_mem.nr_banks; bank++ )
    {
        struct bootmodule *reserved_bootmodule;
        paddr_t bank_start = bootinfo.reserved_mem.bank[bank].start;
        paddr_t bank_size = bootinfo.reserved_mem.bank[bank].size;

        reserved_bootmodule = add_boot_module(BOOTMOD_XEN, bank_start,
                                              bank_size, false);
        BUG_ON(!reserved_bootmodule);
    }

    /*
     * On RISC-V setup_xenheap_mappings() expects to be called with the lowest
     * bank in memory first. There is no requirement that the DT will provide
     * the banks sorted in ascending order. So sort them through.
     */
    sort(bootinfo.mem.bank, bootinfo.mem.nr_banks, sizeof(struct membank),
         cmp_memory_node, generic_swap);

    init_pdx();

    total_pages = 0;
    for ( bank = 0 ; bank < bootinfo.mem.nr_banks; bank++ )
    {
        paddr_t bank_start = bootinfo.mem.bank[bank].start;
        paddr_t bank_size = bootinfo.mem.bank[bank].size;
        paddr_t bank_end = bank_start + bank_size;
        paddr_t bank_split;

        ram_size = ram_size + bank_size;
        ram_start = min(ram_start,bank_start);
        ram_end = max(ram_end,bank_end);

        /*
         * At most SECOND_SIZE of memory can be mapped before alloc_boot_pages
         * is available. Map an area within a single second level leaf pte and
         * enable the boot allocator, and then map the remaining memory.
         */
        bank_split = min(bank_end, ROUNDUP(bank_start + 1, SECOND_SIZE));
        /* It is the first bank and the size of it exceeds one second level megapage */
        if ( bank == 0 && bank_split < bank_end)
        {
            setup_memory_region(bank_start, bank_split);
            setup_memory_region(bank_split, bank_end);
        }
        else
            setup_memory_region(bank_start, bank_end);
    }

    total_pages += ram_size >> PAGE_SHIFT;

    xenheap_virt_end = XENHEAP_VIRT_START + ram_end - ram_start;
    xenheap_mfn_start = maddr_to_mfn(ram_start);
    xenheap_mfn_end = maddr_to_mfn(ram_end);

    setup_frametable_mappings(ram_start, ram_end);
    max_page = PFN_DOWN(ram_end);

    setup_fixmap_mappings();
}

/** start_xen - The C entry point
 *
 * The real entry point is in head.S.
 */
void __init start_xen(paddr_t fdt_paddr, paddr_t boot_phys_offset)
{
    struct domain *dom0;
    struct xen_domctl_createdomain dom0_cfg = {
        .flags = XEN_DOMCTL_CDF_hvm | XEN_DOMCTL_CDF_hap,
        .max_evtchn_port = -1,
        .max_grant_frames = gnttab_dom0_frames(),
        .max_maptrack_frames = -1,
    };
    
    size_t fdt_size;
    const char *cmdline;
    unsigned int i;
    struct bootmodule *xen_bootmodule, *fdt_bootmodule;

    nr_cpu_ids = NR_CPUS;

    percpu_init_areas();

    setup_virtual_regions(NULL, NULL);
    smp_clear_cpu_maps();
    
    device_tree_flattened = early_fdt_map(fdt_paddr);
    if ( !device_tree_flattened )
        panic("Invalid device tree blob at physical address %#lx.\n"
              "The DTB must be 8-byte aligned and must not exceed 2 MB in size.\n\n"
              "Please check your bootloader.\n",
              fdt_paddr);

    fdt_size = boot_fdt_info(device_tree_flattened, fdt_paddr);

    /* Register Xen's load address as a boot module. */
    xen_bootmodule = add_boot_module(BOOTMOD_XEN,
                             (paddr_t)(uintptr_t)(_start + boot_phys_offset),
                             (paddr_t)(uintptr_t)(_end - _start), false);
    /* FDT */
    fdt_bootmodule = add_boot_module(BOOTMOD_FDT,
                                     (paddr_t)(uintptr_t)(fdt_paddr),
                                     (paddr_t)(uintptr_t)(fdt_size), false);
    BUG_ON(!xen_bootmodule || !fdt_bootmodule);
    
    cmdline = boot_fdt_cmdline(device_tree_flattened);
    printk("Command line: %s\n", cmdline);
    cmdline_parse(cmdline);

    setup_mm();
    end_boot_allocator();

    /*
     * system_state is SYS_STATE_boot after the boot allocator has ended and the
     * memory subsystem has been initialized.
     */
    system_state = SYS_STATE_boot;

    vm_init();

    if ( acpi_disabled )
    {
        printk("Booting using Device Tree\n");
        dt_unflatten_host_device_tree();
    }
    else
    {
        panic("TODO: ACPI\n");
        printk("Booting using ACPI\n");
        device_tree_flattened = NULL;
    }

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

    preinit_xen_time();

    plic_preinit();

    uart_init();
    console_init_preirq();
    console_init_ring();

    printk("RISC-V Xen Boot!\n");

    init_xen_time();

    init_timer_interrupt();

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
