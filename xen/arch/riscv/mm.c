/*
 * xen/arch/riscv/mm.c
 *
 * MMU code for a RISC-V RV32/64 with hypervisor extensions.
 *
 * Copyright (c) 2019 Bobby Eshleman <bobbyeshleman@gmail.com>
 *
 * Based on code that is Copyright (c) 2018 Anup Patel.
 * Based on code that is Copyright (c) 2011 Tim Deegan <tim@xen.org>
 * Based on code that is Copyright (c) 2011 Citrix Systems.
 *
 * Parts of this code are based on:
 *     ARM/Xen: xen/arch/arm/mm.c.
 *     Xvisor: arch/riscv/cpu/generic/cpu_mmu_initial_pgtbl.c
 *         (https://github.com/xvisor/xvisor/tree/v0.2.11)
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

#include <xen/compile.h>
#include <xen/types.h>
#include <xen/init.h>
#include <xen/mm.h>
#include <asm/p2m.h>
#include <public/domctl.h>
#include <asm/page.h>
#include <xen/preempt.h>
#include <xen/errno.h>
#include <xen/grant_table.h>
#include <xen/softirq.h>
#include <xen/event.h>
#include <xen/guest_access.h>
#include <xen/domain_page.h>
#include <xen/err.h>
#include <asm/page.h>
#include <asm/current.h>
#include <asm/flushtlb.h>
#include <public/memory.h>
#include <xen/sched.h>
#include <xen/vmap.h>
#include <xsm/xsm.h>
#include <xen/pfn.h>
#include <xen/sizes.h>
#include <asm/setup.h>

#define XEN_TABLE_MAP_FAILED 0
#define XEN_TABLE_SUPER_PAGE 1
#define XEN_TABLE_NORMAL_PAGE 2

enum pt_level {
    pt_level_zero,
    pt_level_one,
#if CONFIG_PAGING_LEVELS == 3
    pt_level_two,
#endif
};

/* Override macros from asm/page.h to make them work with mfn_t */
#undef virt_to_mfn
#define virt_to_mfn(va) _mfn(__virt_to_mfn(va))
#undef mfn_to_virt
#define mfn_to_virt(mfn) __mfn_to_virt(mfn_x(mfn))

/* Limits of the Xen heap */
mfn_t xenheap_mfn_start __read_mostly = INVALID_MFN_INITIALIZER;
mfn_t xenheap_mfn_end __read_mostly;
vaddr_t xenheap_virt_end __read_mostly;
vaddr_t xenheap_virt_start __read_mostly;
unsigned long xenheap_base_pdx __read_mostly;

/* Limits of frametable */
unsigned long frametable_virt_end __read_mostly;
unsigned long frametable_base_pdx;

/*
 * xen_second_pagetable is indexed with the VPN[2] page table entry field
 * xen_first_pagetable is accessed from the VPN[1] page table entry field
 * xen_zeroeth_pagetable is accessed from the VPN[0] page table entry field
 */
pte_t xen_second_pagetable[PAGE_ENTRIES] __attribute__((__aligned__(4096)));
static pte_t xen_first_pagetable[PAGE_ENTRIES]
    __attribute__((__aligned__(4096)));
static pte_t xen_zeroeth_pagetable[PAGE_ENTRIES]
    __attribute__((__aligned__(4096)));
static pte_t xen_heap_megapages[PAGE_ENTRIES]
    __attribute__((__aligned__(4096)));

static pte_t xen_fixmap[PAGE_ENTRIES] __attribute__((__aligned__(4096)));


/*
 * The second level slot which points to xen_heap_megapages.
 * This slot indexes into the PTE that points to the first level table
 * of megapages that we used to map in and then initialize our first
 * set of boot pages.  Once it has been used to map/init boot page,
 * those pages can be used to alloc the rest of the page tables with
 * the alloc_boot_pages().
 */
static __initdata int xen_second_heap_slot = -1;

#define THIS_CPU_PGTABLE xen_second_pagetable

/* Used by _setup_initial_pagetables() and initialized by head.S */
extern unsigned long _text_start;
extern unsigned long _text_end;
extern unsigned long _cpuinit_start;
extern unsigned long _cpuinit_end;
extern unsigned long _spinlock_start;
extern unsigned long _spinlock_end;
extern unsigned long _init_start;
extern unsigned long _init_end;
extern unsigned long _rodata_start;
extern unsigned long _rodata_end;

paddr_t phys_offset;
unsigned long max_page;

static inline pte_t mfn_to_pte(mfn_t mfn)
{
    unsigned long pte = mfn_x(mfn) << PTE_SHIFT;
    return (pte_t){ .pte = pte };
}

void *__init arch_vmap_virt_end(void)
{
    return (void *)VMAP_VIRT_END;
}

static inline pte_t mfn_to_xen_entry(mfn_t mfn)
{
    return mfn_to_pte(mfn);
}

/* Map a 4k page in a fixmap entry */
void set_fixmap(unsigned map, mfn_t mfn, unsigned int flags)
{
    pte_t pte;

    pte = mfn_to_xen_entry(mfn);
    pte.pte |= PTE_LEAF_DEFAULT;
    write_pte(&xen_fixmap[pagetable_zeroeth_index(FIXMAP_ADDR(map))], pte);
}

/* Remove a mapping from a fixmap entry */
void clear_fixmap(unsigned map)
{
    pte_t pte = {0};
    write_pte(&xen_fixmap[pagetable_zeroeth_index(FIXMAP_ADDR(map))], pte);
}

#ifdef CONFIG_DOMAIN_PAGE
void *map_domain_page_global(mfn_t mfn)
{
    return vmap(&mfn, 1);
}

void unmap_domain_page_global(const void *va)
{
    vunmap(va);
}
#endif

void flush_page_to_ram(unsigned long mfn, bool sync_icache)
{
    void *va = map_domain_page(_mfn(mfn));
    unmap_domain_page(va);

    /* TODO */

    if ( sync_icache )
        invalidate_icache();
}

enum xenmap_operation { INSERT, REMOVE, MODIFY, RESERVE };

static pte_t virt_to_pte(void *p)
{
    return mfn_to_xen_entry(maddr_to_mfn(virt_to_maddr(p)));
}

static vaddr_t get_xen_pt_root(void)
{
    return (paddr_t)&xen_second_pagetable[0];
}

static int xen_pt_update(unsigned long va, mfn_t mfn, unsigned int flags)
{
    int rc;

    if ( mfn_eq(mfn, INVALID_MFN) )
    {
        return -EINVAL;
    }

    /* TODO: Support pagetable root for different CPUs (SMP) */
    /* TODO: Use flags */
    rc = pt_update(get_xen_pt_root(),
                     va, mfn_to_maddr(mfn), true, NULL,
                     PTE_READABLE | PTE_WRITABLE | PTE_EXECUTABLE);

    /*
     * Remove this after being confident that pt_update / pt_walk
     * work across the general cases.
     */
    BUG_ON(pt_walk(get_xen_pt_root(), va, true) != mfn_to_maddr(mfn));

    return rc;
}

static DEFINE_SPINLOCK(xen_pt_lock);

int map_pages_to_xen(unsigned long virt, mfn_t mfn, unsigned long nr_mfns,
                     unsigned int flags)
{
    int rc = 0;
    unsigned long addr = virt, addr_end = addr + nr_mfns * PAGE_SIZE;

    if ( !IS_ALIGNED(virt, PAGE_SIZE) )
    {
        dprintk(XENLOG_ERR, "The virtual address is not aligned to the page-size.\n");
        return -EINVAL;
    }

    spin_lock(&xen_pt_lock);
    while ( addr < addr_end )
    {
        rc = xen_pt_update(addr, mfn, flags);
        if ( rc )
            break;

        mfn = mfn_add(mfn, 1);
        addr += PAGE_SIZE;
    }

    /*
     * Flush the TLBs even in case of failure because we may have
     * partially modified the PT. This will prevent any unexpected
     * behavior afterwards.
     */
    asm volatile("sfence.vma");
    spin_unlock(&xen_pt_lock);

    return rc;
}

int populate_pt_range(unsigned long virt, unsigned long nr_mfns)
{
    (void) virt;
    (void) nr_mfns;

    /* TODO */

    return  0;
}

int destroy_xen_mappings(unsigned long v, unsigned long e)
{
    (void) v;
    (void) e;

    /* TODO */

    return 0;
}

int modify_xen_mappings(unsigned long s, unsigned long e, unsigned int flags)
{
    (void) s;
    (void) e;
    (void) flags;

    /* TODO */

    return 0;
}

void arch_dump_shared_mem_info(void)
{
    /* TODO */
}

int donate_page(struct domain *d, struct page_info *page, unsigned int memflags)
{
    ASSERT_UNREACHABLE();
    return -ENOSYS;
}

int steal_page(struct domain *d, struct page_info *page, unsigned int memflags)
{
    return -EOPNOTSUPP;
}

int page_is_ram_type(unsigned long mfn, unsigned long mem_type)
{
    ASSERT_UNREACHABLE();
    return 0;
}

unsigned long domain_get_maximum_gpfn(struct domain *d)
{
    BUG();
    return 0;
}

void share_xen_page_with_guest(struct page_info *page, struct domain *d,
                               enum XENSHARE_flags flags)
{
    if ( page_get_owner(page) == d )
        return;

    spin_lock(&d->page_alloc_lock);

    /* TODO */

    spin_unlock(&d->page_alloc_lock);
}

int xenmem_add_to_physmap_one(struct domain *d, unsigned int space,
                              union add_to_physmap_extra extra,
                              unsigned long idx, gfn_t gfn)
{
    /* TODO */

    return 0;
}

long arch_memory_op(int op, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    /* TODO */
    return 0;
}

struct domain *page_get_owner_and_reference(struct page_info *page)
{
    unsigned long x, y = page->count_info;
    struct domain *owner;

    do
    {
        x = y;
        /*
         * Count ==  0: Page is not allocated, so we cannot take a reference.
         * Count == -1: Reference count would wrap, which is invalid.
         */
        if ( unlikely(((x + 1) & PGC_count_mask) <= 1) )
            return NULL;
    } while ( (y = cmpxchg(&page->count_info, x, x + 1)) != x );

    owner = page_get_owner(page);
    ASSERT(owner);

    return owner;
}

void put_page(struct page_info *page)
{
    unsigned long nx, x, y = page->count_info;

    do
    {
        ASSERT((y & PGC_count_mask) != 0);
        x = y;
        nx = x - 1;
    } while ( unlikely((y = cmpxchg(&page->count_info, x, nx)) != x) );

    if ( unlikely((nx & PGC_count_mask) == 0) )
    {
        free_domheap_page(page);
    }
}

bool get_page(struct page_info *page, const struct domain *domain)
{
    struct domain *owner = page_get_owner_and_reference(page);

    if ( likely(owner == domain) )
        return true;

    if ( owner != NULL )
        put_page(page);

    return false;
}

/* Common code requires get_page_type and put_page_type.
 * We don't care about typecounts so we just do the minimum to make it
 * happy. */
int get_page_type(struct page_info *page, unsigned long type)
{
    return 1;
}

void put_page_type(struct page_info *page)
{
    return;
}

/*
 * This function should only be used to remap device address ranges
 * TODO: add a check to verify this assumption
 */
void __iomem *ioremap_attr(paddr_t pa, size_t len, unsigned int attributes)
{
    mfn_t mfn = _mfn(PFN_DOWN(pa));
    unsigned int offs = pa & (PAGE_SIZE - 1);
    unsigned int nr = PFN_UP(offs + len);

    void *ptr = __vmap(&mfn, nr, 1, 1, attributes, VMAP_DEFAULT);

    if ( ptr == NULL )
        return NULL;

    return ptr + offs;
}

void *ioremap(paddr_t pa, size_t len)
{
    return ioremap_attr(pa, len, PAGE_HYPERVISOR_NOCACHE);
}

#ifdef CONFIG_GRANT_TABLE
void gnttab_clear_flags(struct domain *d, unsigned long nr, uint16_t *addr)
{
	/* TODO */
}

void gnttab_mark_dirty(struct domain *d, mfn_t mfn)
{
	/* TODO */
}

int create_grant_host_mapping(unsigned long addr, mfn_t frame,
                              unsigned int flags, unsigned int cache_flags)
{
	/* TODO */
}

int replace_grant_host_mapping(unsigned long addr, mfn_t mfn,
                               unsigned long new_addr, unsigned int flags)
{
	/* TODO */
}
#endif

bool is_iomem_page(mfn_t mfn)
{
    return !mfn_valid(mfn);
}

unsigned long get_upper_mfn_bound(void)
{
    /* No memory hotplug yet, so current memory limit is the final one. */
    return max_page - 1;
}

static void setup_second_level_mappings(pte_t *first_pagetable,
                                        unsigned long vaddr)
{
    unsigned long paddr;
    unsigned long index;
    pte_t *p;

    index = pagetable_second_index(vaddr);
    p = &xen_second_pagetable[index];

    if ( !pte_is_valid(p) )
    {
        paddr = phys_offset + ((unsigned long)first_pagetable);
        p->pte = addr_to_ppn(paddr);
        p->pte |= PTE_TABLE;
    }
}

void setup_megapages(pte_t *first_pagetable, unsigned long virtual_start,
                     unsigned long physical_start, unsigned long page_cnt)
{
    unsigned long frame_addr = physical_start;
    unsigned long end = physical_start + (page_cnt << PAGE_SHIFT);
    unsigned long vaddr = virtual_start;
    unsigned long index;
    pte_t *p;

    BUG_ON(!IS_ALIGNED(physical_start, FIRST_SIZE));

    while ( frame_addr < end )
    {
        setup_second_level_mappings(first_pagetable, vaddr);

        index = pagetable_first_index(vaddr);
        p = &first_pagetable[index];
        p->pte = paddr_to_megapage_ppn(frame_addr);
        p->pte |= PTE_LEAF_DEFAULT;

        frame_addr += FIRST_SIZE;
        vaddr += FIRST_SIZE;
    }

    asm volatile("sfence.vma");
}

void setup_fixmap_mappings(void)
{
    pte_t *second, *first;
    pte_t pte;

    second = &xen_second_pagetable[pagetable_second_index(FIXMAP_ADDR(0))];

    BUG_ON( !pte_is_valid(second) );

    first = (pte_t*)pte_to_paddr(second);
    first = &first[pagetable_first_index(FIXMAP_ADDR(0))];

    if ( !pte_is_valid(first) ) {
        pte = virt_to_pte(&xen_fixmap);
        pte.pte |= PTE_TABLE;

        write_pte(first, pte);
    }

    /*
     * We only need the zeroeth table allocated, but not the PTEs set, because
     * set_fixmap() will set them on the fly.
     */
}

/*
 * Convert a virtual address to a PTE with the correct PPN.
 *
 * WARNING: Only use this function while the physical addresses
 * of Xen are still mapped in as virtual addresses OR before
 * the MMU is enabled (i.e., phys_offset must still be valid).
 */
static inline pte_t pte_of_xenaddr(vaddr_t va)
{
    paddr_t ma = va + phys_offset;
    return mfn_to_xen_entry(maddr_to_mfn(ma));
}

/* Creates megapages of 2MB size based on sv39 spec */
void __init setup_xenheap_mappings(unsigned long base_mfn,
                                   unsigned long nr_mfns)
{
    unsigned long mfn, end_mfn;
    vaddr_t vaddr;
    pte_t *first, pte;

    /* The most that this can possibly map is 1GB */
    BUG_ON(nr_mfns > (GB(1) >> PAGE_SHIFT));

    /* Align to previous 2MB boundary */
    mfn = base_mfn & ~((FIRST_SIZE >> PAGE_SHIFT) - 1);

    /* First call sets the xenheap physical and virtual offset. */
    if ( mfn_eq(xenheap_mfn_start, INVALID_MFN) )
    {
        xenheap_mfn_start = _mfn(base_mfn);
        xenheap_base_pdx = mfn_to_pdx(_mfn(base_mfn));
        xenheap_virt_start =
            DIRECTMAP_VIRT_START + (base_mfn - mfn) * PAGE_SIZE;
    }

    if ( base_mfn < mfn_x(xenheap_mfn_start) )
        panic("cannot add xenheap mapping at %lx below heap start %lx\n",
              base_mfn, mfn_x(xenheap_mfn_start));

    end_mfn = base_mfn + nr_mfns;

    /*
     * Virtual address aligned to previous 2MB to match physical
     * address alignment done above.
     */
    vaddr = (vaddr_t)__mfn_to_virt(base_mfn) & (SECOND_MASK | FIRST_MASK);

    while ( mfn < end_mfn )
    {
        unsigned long slot = pagetable_second_index(vaddr);
        pte_t *p = &xen_second_pagetable[slot];

        if ( pte_is_valid(p) )
        {
            /* mfn_to_virt is not valid on the xen_heap_megapages mfn, since it
             * is not within the xenheap. */
            first = (slot == xen_second_heap_slot)
                        ? xen_heap_megapages
                        : mfn_to_virt(pte_get_mfn(*p));
        }
        else if ( xen_second_heap_slot == -1 )
        {
            /* Use xen_heap_megapages to bootstrap the mappings */
            first = xen_heap_megapages;
            pte = pte_of_xenaddr((vaddr_t)xen_heap_megapages);
            pte.pte |= PTE_TABLE;
            write_pte(p, pte);
            xen_second_heap_slot = slot;
        }
        else
        {
            mfn_t first_mfn = alloc_boot_pages(1, 1);
            clear_page(mfn_to_virt(first_mfn));
            pte = mfn_to_xen_entry(first_mfn);
            pte.pte |= PTE_TABLE;
            write_pte(p, pte);
            first = mfn_to_virt(first_mfn);
        }

        pte = mfn_to_xen_entry(_mfn(mfn));
        pte.pte |= PTE_LEAF_DEFAULT;
        write_pte(&first[pagetable_first_index(vaddr)], pte);

        /*
         * We are mapping pages at the 2MB first-level granularity, so increment
         * by FIRST_SIZE.
         */
        mfn += FIRST_SIZE >> PAGE_SHIFT;
        vaddr += FIRST_SIZE;
    }

    asm volatile("sfence.vma");
}

#define resolve_early_addr(x) \
    ({                                                                          \
         unsigned long * __##x;                                                 \
        if ( load_addr_start <= x && x < load_addr_end )                        \
            __##x = (unsigned long *)x;                                         \
        else                                                                    \
            __##x = (unsigned long *)(x + load_addr_start - linker_addr_start); \
        __##x;                                                                  \
     })

void __init clear_pagetables(unsigned long load_addr_start,
                             unsigned long load_addr_end,
                             unsigned long linker_addr_start,
                             unsigned long linker_addr_end)
{
    unsigned long *p;
    unsigned long page;
    unsigned long i;

    page = (unsigned long)&xen_second_pagetable[0];

    p = resolve_early_addr(page);
    for ( i = 0; i < ARRAY_SIZE(xen_second_pagetable); i++ )
    {
        p[i] = 0ULL;
    }

    page = (unsigned long)&xen_first_pagetable[0];
    p = resolve_early_addr(page);
    for ( i = 0; i < ARRAY_SIZE(xen_first_pagetable); i++ )
    {
        p[i] = 0ULL;
    }

    page = (unsigned long)&xen_zeroeth_pagetable[0];
    p = resolve_early_addr(page);
    for ( i = 0; i < ARRAY_SIZE(xen_zeroeth_pagetable); i++ )
    {
        p[i] = 0ULL;
    }
}

void __attribute__((section(".entry")))
setup_initial_pagetables(pte_t *second, pte_t *first, pte_t *zeroeth,
                         unsigned long map_start, unsigned long map_end,
                         unsigned long pa_start)
{
    unsigned long page_addr;
    unsigned long index2;
    unsigned long index1;
    unsigned long index0;

    /* align start addresses */
    map_start &= ZEROETH_MAP_MASK;
    pa_start &= ZEROETH_MAP_MASK;

    page_addr = map_start;
    while ( page_addr < map_end )
    {
        index2 = pagetable_second_index(page_addr);
        index1 = pagetable_first_index(page_addr);
        index0 = pagetable_zeroeth_index(page_addr);

        /* Setup level2 table */
        second[index2] = paddr_to_pte((unsigned long)first);
        second[index2].pte |= PTE_TABLE;

        /* Setup level1 table */
        first[index1] = paddr_to_pte((unsigned long)zeroeth);
        first[index1].pte |= PTE_TABLE;

        /* Setup level0 table */
        if ( !pte_is_valid(&zeroeth[index0]) )
        {
            /* Update level0 table */
            zeroeth[index0] = paddr_to_pte((page_addr - map_start) + pa_start);
            zeroeth[index0].pte |= PTE_LEAF_DEFAULT;
        }

        /* Point to next page */
        page_addr += ZEROETH_SIZE;
    }
}

/*
 * WARNING: load_addr() and linker_addr() are to be called only when the MMU is
 * disabled and only when executed by the primary CPU.  They cannot refer to
 * any global variable or functions.
 */

/*
 * Convert an addressed layed out at link time to the address where it was loaded
 * by the bootloader.
 */
#define load_addr(linker_address)                                              \
    ({                                                                         \
        unsigned long __linker_address = (unsigned long)(linker_address);      \
        if ( linker_addr_start <= __linker_address &&                           \
            __linker_address < linker_addr_end )                                \
        {                                                                      \
            __linker_address =                                                 \
                __linker_address - linker_addr_start + load_addr_start;        \
        }                                                                      \
        __linker_address;                                                      \
    })

/* Convert boot-time Xen address from where it was loaded by the boot loader to the address it was layed out
 * at link-time.
 */
#define linker_addr(load_address)                                              \
    ({                                                                         \
        unsigned long __load_address = (unsigned long)(load_address);          \
        if ( load_addr_start <= __load_address &&                               \
            __load_address < load_addr_end )                                    \
        {                                                                      \
            __load_address =                                                   \
                __load_address - load_addr_start + linker_addr_start;          \
        }                                                                      \
        __load_address;                                                        \
    })

/*
 * _setup_initial_pagetables:
 *
 * 1) Build the page tables for Xen that map the following:
 *   1.1)  The physical location of Xen (where the bootloader loaded it)
 *   1.2)  The link-time location of Xen (where the linker expected Xen's
 *         addresses to be)
 * 2) Load the page table into the SATP and enable the MMU
 */
void __attribute__((section(".entry")))
_setup_initial_pagetables(unsigned long load_addr_start,
                          unsigned long load_addr_end,
                          unsigned long linker_addr_start,
                          unsigned long linker_addr_end)
{
    pte_t *second;
    pte_t *first;
    pte_t *zeroeth;

    clear_pagetables(load_addr_start, load_addr_end,
                     linker_addr_start, linker_addr_end);

    /* Get the addresses where the page tables were loaded */
    second = (pte_t *)load_addr(&xen_second_pagetable);
    first = (pte_t *)load_addr(&xen_first_pagetable);
    zeroeth = (pte_t *)load_addr(&xen_zeroeth_pagetable);

    /*
     * Create a mapping of the load time address range to... the load time address range.
     * This mapping is used at boot time only.
     */
    setup_initial_pagetables(second, first, zeroeth, load_addr_start,
                             load_addr_end, load_addr_start);

    /*
     * Create a mapping from Xen's link-time addresses to where they were actually loaded.
     *
     * TODO: Protect regions accordingly (e.g., protect text and rodata from writes).
     */
    setup_initial_pagetables(second, first, zeroeth, linker_addr(&_text_start),
                             linker_addr(&_text_end), load_addr(&_text_start));
    setup_initial_pagetables(second, first, zeroeth, linker_addr(&_init_start),
                             linker_addr(&_init_end), load_addr(&_init_start));
    setup_initial_pagetables(second, first, zeroeth,
                             linker_addr(&_cpuinit_start),
                             linker_addr(&_cpuinit_end),
                             load_addr(&_cpuinit_start));
    setup_initial_pagetables(second, first, zeroeth,
                             linker_addr(&_spinlock_start),
                             linker_addr(&_spinlock_end),
                             load_addr(&_spinlock_start));
    setup_initial_pagetables(second, first, zeroeth,
                             linker_addr(&_rodata_start),
                             linker_addr(&_rodata_end),
                             load_addr(&_rodata_start));
    setup_initial_pagetables(second, first, zeroeth, linker_addr_start,
                             linker_addr_end, load_addr_start);

    /* Ensure page table writes precede loading the SATP */
    asm volatile("sfence.vma");

    /* Enable the MMU and load the new pagetable for Xen */
    csr_write(CSR_SATP,
              (load_addr(xen_second_pagetable) >> PAGE_SHIFT) | SATP_MODE);

    phys_offset = load_addr_start > linker_addr_start ?
                      load_addr_start - linker_addr_start :
                      linker_addr_start - load_addr_start;
}

/* Map a frame table to cover physical addresses ps through pe */
void __init setup_frametable_mappings(paddr_t ps, paddr_t pe)
{
    unsigned long nr_pdxs = mfn_to_pdx(mfn_add(maddr_to_mfn(pe), -1)) -
                            mfn_to_pdx(maddr_to_mfn(ps)) + 1;
    unsigned long frametable_size = nr_pdxs * sizeof(struct page_info);
    unsigned long virt_end;
    pte_t *first_table;
    mfn_t mfn, base, first;
    pte_t pte;
    unsigned long i, first_entries_remaining;

    frametable_base_pdx = mfn_to_pdx(maddr_to_mfn(ps));

    /* Allocate enough pages to hold the whole address space */
    base = alloc_boot_pages(frametable_size >> PAGE_SHIFT, MB(2) >> PAGE_SHIFT);
    virt_end = FRAMETABLE_VIRT_START + frametable_size;

    first_entries_remaining = 0;
    mfn = base;

    /* Map the frametable virtual address speace to thse pages */
    for ( i = ROUNDUP(FRAMETABLE_VIRT_START, MB(2)); i < virt_end; i += MB(2) )
    {
        /* If this frame has filled up all entries, then allocate a new table */
        if ( first_entries_remaining <= 0 )
        {
            /* Allocate page for a first-level table */
            first = alloc_boot_pages(1, 1);

            /* TODO: add clear_page(mfn_to_virt(first)); */

            /* Reset counter */
            first_entries_remaining = 512;
        }

        /* Convert the first-level table from it's machine frame number to a virtual_address */
        first_table = (pte_t *)mfn_to_virt(first);

        pte = mfn_to_xen_entry(mfn);
        pte.pte |= PTE_LEAF_DEFAULT;

        /* Point the first-level table to the machine frame */
        write_pte(&first_table[pagetable_first_index(i)], pte);

        /* Convert the first-level table address into a PTE */
        pte = mfn_to_xen_entry(maddr_to_mfn(virt_to_maddr(&first_table[0])));
        pte.pte |= PTE_TABLE;

        /* Point the second-level table to the first-level table */
        write_pte(&xen_second_pagetable[pagetable_second_index(i)], pte);

        /* First-level tables are at a 2MB granularity so go to the next 2MB page */
        mfn = mfn_add(mfn, MB(2) >> PAGE_SHIFT);

        /* We've used an entry, so decrement the counter */
        first_entries_remaining--;
    }

    memset(&frame_table[0], 0, nr_pdxs * sizeof(struct page_info));
    memset(&frame_table[nr_pdxs], -1,
           frametable_size - (nr_pdxs * sizeof(struct page_info)));

    frametable_virt_end =
        FRAMETABLE_VIRT_START + (nr_pdxs * sizeof(struct page_info));
}

/*
 * Map the table that pte points to.
 */
void *map_xen_table(pte_t *pte)
{
    return (pte_t*)maddr_to_virt(pte_to_paddr(pte));
}

/*
 * Map the table that pte points to.
 */
void *map_domain_table(pte_t *pte)
{
    return map_domain_page(maddr_to_mfn((paddr_t)pte_to_paddr(pte)));
}

void unmap_domain_table(pte_t *table)
{
    return unmap_domain_page(table);
}

static inline bool is_super_page(pte_t *pte, enum pt_level level)
{
    if ( !pte )
        return false;

    return pte_is_valid(pte) && !pte_is_table(pte) && level != pt_level_zero;
}

static void clean_page_info(struct page_info *page)
{
    void *p = __map_domain_page(page);

    clear_page(p);
    unmap_domain_page(p);
}

/* Creates a table using the correct allocator */
static int create_table(pte_t *pte, bool use_xenheap, struct domain *d)
{
    pte_t new;
    paddr_t phys_addr;

    BUG_ON( !d && !use_xenheap );
    BUG_ON( SYS_STATE_boot <= SYS_STATE_early_boot );
    BUG_ON( !pte );

    if ( !pte )
        return -EINVAL;

    if ( use_xenheap )
    {
        void *new_table = alloc_xenheap_page();

        if ( !new_table )
            return -ENOMEM;

        clear_page(new_table);
        phys_addr = virt_to_maddr(new_table);
    }
    else
    {
        struct page_info *page = alloc_domheap_pages(NULL, 1, 0);

        if ( !page )
            return -ENOMEM;

        page_list_add(page, &p2m_get_hostp2m(d)->pages);
        clean_page_info(page);
        phys_addr = page_to_maddr(page);
    }

    BUG_ON( !phys_addr );

    new = paddr_to_pte(phys_addr);
    new.pte |= PTE_TABLE;
    write_pte(pte, new);

    return 0;
}

/*
 * Returns the page table pointed to by entry in table, indexed by va.
 *
 * table: the current page table
 * va: the virtual address to be mapped from
 * current_level: the level of arg table (l2, l1, l0 for sv39)
 * use_xenheap: use the xen heap if yes, otherwise yes
 *              the dom heap for allocating new tables
 * d: the domain the page table is for
 *
 * d is not used if uxe_xenheap == true.
 *
 * The table returned is mapped in (by map_domain_page if !use_xenheap, or
 * automatically if from xenheap), therefore if !use_xenheap, then the caller
 * must unmap the table using unmap_domain_page() after use.
 *
 * Returns the virtual address to the table.
 */
static pte_t *pt_next_level(pte_t *table, vaddr_t va, enum pt_level current_level,
                            bool use_xenheap, struct domain *d)
{
    pte_t *pte;
    unsigned long index;
    int rc;

    BUG_ON( SYS_STATE_boot <= SYS_STATE_early_boot );

    switch ( current_level )
    {
        case pt_level_two:
            index = pagetable_second_index(va);
            break;
        case pt_level_one:
            index = pagetable_first_index(va);
            break;
        case pt_level_zero:
        default:
            BUG();
            break;
    }

    pte = &table[index];

    if ( is_super_page(pte, current_level) )
    {
        printk(XENLOG_ERR "Breaking up super pages not supported\n");
        return ERR_PTR(-EOPNOTSUPP);
    }

    if ( !pte_is_table(pte) && current_level != pt_level_zero )
    {
        rc = create_table(pte, use_xenheap, d);

        if ( rc )
            return ERR_PTR(rc);
    }

    if ( use_xenheap )
        return (pte_t*)maddr_to_virt(pte_to_paddr(pte));

    return (pte_t*)map_domain_page(maddr_to_mfn(pte_to_paddr(pte)));
}

/*
 * Updates the page tables found at root with a mapping
 * from va to pa.
 *
 * root: the virtual address of the top level page table
 * va: the virtual address to be mapped from
 * pa: the physical address to be mapped to
 * use_xenheap: use the xen heap if yes, otherwise yes
 *              the dom heap for allocating new tables
 * d: the domain the page table is for
 *
 * d is not used if uxe_xenheap == true.
 *
 * Returns 0 on success, otherwise returns negative errno.
 */
int pt_update(vaddr_t root, vaddr_t va, paddr_t pa,
              bool use_xenheap, struct domain *d, unsigned long flags)
{
    pte_t *l2, *l1, *l0, new;

    BUG_ON( !root );
    BUG_ON( SYS_STATE_boot <= SYS_STATE_early_boot );
    BUILD_BUG_ON( CONFIG_PAGING_LEVELS != 3 );

    /* Level 2 */
    l2 = (pte_t*)root;
    l1 = pt_next_level(l2, va, pt_level_two, use_xenheap, d);

    if ( IS_ERR(l1) )
        return PTR_ERR(l1);

    /* Level 1 */
    l0 = pt_next_level(l1, va, pt_level_one, use_xenheap, d);

    if ( IS_ERR(l0) )
        return PTR_ERR(l0);

    /* Level 0 */
    new = paddr_to_pte(pa);
    new.pte |= PTE_VALID | flags;
    write_pte(&l0[pagetable_zeroeth_index(va)], new);

    if ( !use_xenheap )
    {
        unmap_domain_page(l1);
        unmap_domain_page(l0);
    }

    return 0;
}

/*
 * Returns a virtual to physical address mapping.
 *
 * root:   virtual address of the page table
 * va:     the virtual address
 * is_xen: set to true if the tables are off the xen heap, otherwise false.
 */
paddr_t pt_walk(vaddr_t root, vaddr_t va, bool is_xen)
{
    paddr_t pa;
    pte_t *second, *first, *zeroeth;
    unsigned long index0, index1, index2;

    BUILD_BUG_ON(CONFIG_PAGING_LEVELS != 3);

    second = (pte_t*)root;
    index2 = pagetable_second_index(va);

    if ( !pte_is_valid(&second[index2]) || !pte_is_table(&second[index2]) )
    {
        pa = 0;
        goto out;
    }

    first = &second[index2];
    first = is_xen ? map_xen_table(first) : map_domain_table(first);

    index1 = pagetable_first_index(va);

    if ( !pte_is_valid(&first[index1]) || !pte_is_table(&first[index1]) )
    {
        pa = 0;
        goto out;
    }

    zeroeth = &first[index1];
    zeroeth = is_xen ? map_xen_table(zeroeth) : map_domain_table(zeroeth);

    index0 = pagetable_zeroeth_index(va);

    if ( !pte_is_valid(&zeroeth[index0]) )
    {
        pa = 0;
        goto out;
    }

    pa = pte_to_paddr(&zeroeth[index0]) | (va & (PAGE_SIZE - 1));

out:
    if ( !is_xen ) {
        unmap_domain_table(second);
        unmap_domain_table(first);
        unmap_domain_table(zeroeth);
    }
    return pa;
}

paddr_t __virt_to_maddr(vaddr_t va)
{
    paddr_t maddr = 0;
    unsigned long heap_phys_start = mfn_to_maddr(xenheap_mfn_start);

    /* TODO: what about the domheap?  */

    if ( va > XENHEAP_VIRT_START ) {
        /* va points into the heap */
        maddr = va - XENHEAP_VIRT_START + heap_phys_start;
    } else if ( va < XEN_VIRT_END ) {
        /* va points into Xen, so just apply the load offset */
        /* WARNING: will only work with no relocation */
        maddr = va + phys_offset;
    } else {
        maddr = pt_walk(get_xen_pt_root(), va, true);
    }

    return maddr;
}

int guest_physmap_mark_populate_on_demand(struct domain *d, unsigned long gfn,
                                          unsigned int order)
{
    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
