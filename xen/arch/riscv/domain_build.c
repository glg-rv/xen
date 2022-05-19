#include <asm/domain_build.h>
#include <asm/guest_access.h>
#include <xen/domain.h>
#include <xen/vmap.h>
#include <xen/param.h>
#include <xen/warning.h>
#include <xen/libfdt/libfdt.h>
#include <asm-riscv/acpi.h>

static u64 __initdata dom0_mem;
static bool __initdata dom0_mem_set;

static int __init parse_dom0_mem(const char *s)
{
    dom0_mem_set = true;

    dom0_mem = parse_size_and_unit(s, &s);

    return *s ? -EINVAL : 0;
}
custom_param("dom0_mem", parse_dom0_mem);


static unsigned int __init get_allocation_size(paddr_t size)
{
    /*
     * get_order_from_bytes returns the order greater than or equal to
     * the given size, but we need less than or equal. Adding one to
     * the size pushes an evenly aligned size into the next order, so
     * we can then unconditionally subtract 1 from the order which is
     * returned.
     */
    return get_order_from_bytes(size + 1) - 1;
}

static bool __init allocate_bank_memory(struct domain *d,
                                        struct kernel_info *kinfo,
                                        gfn_t sgfn,
                                        paddr_t tot_size)
{
    int res;
    struct page_info *pg;
    struct membank *bank;
    unsigned int max_order = ~0;

    bank = &kinfo->mem.bank[kinfo->mem.nr_banks];
    bank->start = gfn_to_gaddr(sgfn);
    bank->size = tot_size;

    while ( tot_size > 0 )
    {
        unsigned int order = get_allocation_size(tot_size);

        order = min(max_order, order);

        pg = alloc_domheap_pages(d, order, 0);
        if ( !pg )
        {
            /*
             * If we can't allocate one page, then it is unlikely to
             * succeed in the next iteration. So bail out.
             */
            if ( !order )
                return false;

            /*
             * If we can't allocate memory with order, then it is
             * unlikely to succeed in the next iteration.
             * Record the order - 1 to avoid re-trying.
             */
            max_order = order - 1;
            continue;
        }

        res = guest_physmap_add_page(d, sgfn, page_to_mfn(pg), order);
        if ( res )
        {
            dprintk(XENLOG_ERR, "Failed map pages to DOMU: %d", res);
            return false;
        }

        sgfn = gfn_add(sgfn, 1UL << order);
        tot_size -= (1ULL << (PAGE_SHIFT + order));
    }

    kinfo->mem.nr_banks++;
    kinfo->unassigned_mem -= bank->size;

    return true;
}


static void __init allocate_memory(struct domain *d, struct kernel_info *kinfo)
{
    unsigned int i;
    paddr_t bank_size;

    printk(XENLOG_INFO "Allocating mappings totalling %ldMB for %pd:\n",
           /* Don't want format this as PRIpaddr (16 digit hex) */
           (unsigned long)(kinfo->unassigned_mem >> 20), d);

    kinfo->mem.nr_banks = 0;
    bank_size = MIN(GUEST_RAM0_SIZE, kinfo->unassigned_mem);
    if ( !allocate_bank_memory(d, kinfo, gaddr_to_gfn(GUEST_RAM0_BASE),
                               bank_size) )
        goto fail;

    if ( kinfo->unassigned_mem )
        goto fail;

    for( i = 0; i < kinfo->mem.nr_banks; i++ )
    {
        printk(XENLOG_INFO "%pd BANK[%d] %#"PRIpaddr"-%#"PRIpaddr" (%ldMB)\n",
               d,
               i,
               kinfo->mem.bank[i].start,
               kinfo->mem.bank[i].start + kinfo->mem.bank[i].size,
               /* Don't want format this as PRIpaddr (16 digit hex) */
               (unsigned long)(kinfo->mem.bank[i].size >> 20));
    }

    return;

fail:
    panic("Failed to allocate requested domain memory."
          /* Don't want format this as PRIpaddr (16 digit hex) */
          " %ldKB unallocated. Fix the VMs configurations.\n",
          (unsigned long)kinfo->unassigned_mem >> 10);
}

/*
 * Amount of extra space required to dom0's device tree.  No new nodes
 * are added (yet) but one terminating reserve map entry (16 bytes) is
 * added.
 */
#define DOM0_FDT_EXTRA_SIZE (128 + sizeof(struct fdt_reserve_entry))

static void __init dtb_load(struct kernel_info *kinfo)
{
    unsigned long left;

    printk("Loading %pd DTB to 0x%"PRIpaddr"-0x%"PRIpaddr"\n",
           kinfo->d, kinfo->dtb_paddr,
           kinfo->dtb_paddr + fdt_totalsize(kinfo->fdt));

    left = copy_to_guest_phys(kinfo->d, kinfo->dtb_paddr,
                              kinfo->fdt,
                              fdt_totalsize(kinfo->fdt));

    if ( left != 0 )
        panic("Unable to copy the DTB to %pd memory (left = %lu bytes)\n",
              kinfo->d, left);

    xfree(kinfo->fdt);
}

static void __init initrd_load(struct kernel_info *kinfo)
{
    const struct bootmodule *mod = kinfo->initrd_bootmodule;
    paddr_t load_addr = kinfo->initrd_paddr;
    paddr_t paddr, len;
    int node;
    int res;
    __be32 val[2];
    __be32 *cellp;
    void __iomem *initrd;

    if ( !mod || !mod->size )
        return;

    paddr = mod->start;
    len = mod->size;

    printk("Loading %pd initrd from %"PRIpaddr" to 0x%"PRIpaddr"-0x%"PRIpaddr"\n",
           kinfo->d, paddr, load_addr, load_addr + len);

    /* Fix up linux,initrd-start and linux,initrd-end in /chosen */
    node = fdt_path_offset(kinfo->fdt, "/chosen");
    if ( node < 0 )
        panic("Cannot find the /chosen node\n");

    cellp = (__be32 *)val;
    dt_set_cell(&cellp, ARRAY_SIZE(val), load_addr);
    res = fdt_setprop_inplace(kinfo->fdt, node, "linux,initrd-start",
                              val, sizeof(val));
    if ( res )
        panic("Cannot fix up \"linux,initrd-start\" property\n");

    cellp = (__be32 *)val;
    dt_set_cell(&cellp, ARRAY_SIZE(val), load_addr + len);
    res = fdt_setprop_inplace(kinfo->fdt, node, "linux,initrd-end",
                              val, sizeof(val));
    if ( res )
        panic("Cannot fix up \"linux,initrd-end\" property\n");

    initrd = ioremap_wc(paddr, len);
    if ( !initrd )
        panic("Unable to map the hwdom initrd\n");

    res = copy_to_guest_phys(kinfo->d, load_addr, initrd, len);
    if ( res != 0 )
        panic("Unable to copy the initrd in the hwdom memory\n");
}

static int __init construct_domain(struct domain *d, struct kernel_info *kinfo)
{
    unsigned int i;
    struct vcpu *v = d->vcpu[0];
    struct cpu_user_regs *regs = &v->arch.cpu_info->guest_cpu_user_regs;

    BUG_ON(d->vcpu[0] == NULL);
    BUG_ON(v->is_initialised);

    kernel_load(kinfo);
    initrd_load(kinfo);
    dtb_load(kinfo);

    memset(regs, 0, sizeof(*regs));

    regs->sepc = (register_t)kinfo->entry;

    /* guest boot hart ID = 0 */
    regs->a0 = 0;
    regs->a1 = kinfo->dtb_paddr;

    for ( i = 1; i < d->max_vcpus; i++ )
    {
        if ( vcpu_create(d, i) == NULL )
        {
            printk("Failed to allocate d%dv%d\n", d->domain_id, i);
            break;
        }
        else
        {
            printk("Created vcpu %d for d%d\n", i, d->domain_id);
        }
    }

    v->is_initialised = 1;
    clear_bit(_VPF_down, &v->pause_flags);
    return 0;
}

static int __init handle_node(struct domain *d, struct kernel_info *kinfo,
                              struct dt_device_node *node,
                              p2m_type_t p2mt)
{
    /* TODO */
    return 0;
}

static int __init prepare_dtb_hwdom(struct domain *d, struct kernel_info *kinfo)
{
    const p2m_type_t default_p2mt = p2m_mmio_direct_c;
    const void *fdt;
    int new_size;
    int ret;

    ASSERT(dt_host && (dt_host->sibling == NULL));

    fdt = device_tree_flattened;

    new_size = fdt_totalsize(fdt) + DOM0_FDT_EXTRA_SIZE;
    kinfo->fdt = xmalloc_bytes(new_size);
    if ( kinfo->fdt == NULL )
        return -ENOMEM;

    ret = fdt_create(kinfo->fdt, new_size);
    if ( ret < 0 )
        goto err;

    fdt_finish_reservemap(kinfo->fdt);

    ret = handle_node(d, kinfo, dt_host, default_p2mt);
    if ( ret )
        goto err;

    ret = fdt_finish(kinfo->fdt);
    if ( ret < 0 )
        goto err;

    return 0;

  err:
    printk("Device tree generation failed (%d).\n", ret);
    xfree(kinfo->fdt);
    return -EINVAL;
}

int __init construct_dom0(struct domain *d)
{
    struct kernel_info kinfo = {};
    int rc;

    /* Sanity! */
    BUG_ON(d->domain_id != 0);

    printk("*** LOADING DOMAIN 0 ***\n");

    /* The ordering of operands is to work around a clang5 issue. */
    if ( CONFIG_DOM0_MEM[0] && !dom0_mem_set )
        parse_dom0_mem(CONFIG_DOM0_MEM);

    if ( dom0_mem <= 0 )
    {
        warning_add("PLEASE SPECIFY dom0_mem PARAMETER - USING 512M FOR NOW\n");
        dom0_mem = MB(512);
    }

    d->max_pages = dom0_mem >> PAGE_SHIFT;

    kinfo.unassigned_mem = dom0_mem;
    kinfo.d = d;

    rc = kernel_probe(&kinfo, NULL);
    if ( rc < 0 )
        return rc;

    allocate_memory(d, &kinfo);

    if ( acpi_disabled )
        rc = prepare_dtb_hwdom(d, &kinfo);
    else
        panic("TODO: ACPI\n");
        /* rc = prepare_acpi(d, &kinfo); */

    if ( rc < 0 )
        return rc;

    return construct_domain(d, &kinfo);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
