#include <xen/cpu.h>
#include <xen/domain_page.h>
#include <xen/iocap.h>
#include <xen/lib.h>
#include <xen/sched.h>
#include <xen/softirq.h>
#include <asm/event.h>
#include <asm/flushtlb.h>
#include <asm/page.h>

#define UNIMPLEMENTED() do {                    \
    printk("%s: unimplemented\n", __func__);    \
    BUG();                                      \
    } while ( 0 )

#define INVALID_VMID 0 /* VMID 0 is reserved */

/* Unlock the flush and do a P2M TLB flush if necessary */
void p2m_write_unlock(struct p2m_domain *p2m)
{
    write_unlock(&p2m->lock);
}

void p2m_dump_info(struct domain *d)
{
    struct p2m_domain *p2m = p2m_get_hostp2m(d);

    p2m_read_lock(p2m);
    p2m_read_unlock(p2m);
}

void memory_type_changed(struct domain *d)
{
}

void dump_p2m_lookup(struct domain *d, paddr_t addr)
{
    printk("dom%d IPA 0x%"PRIpaddr"\n", d->domain_id, addr);
}

void p2m_save_state(struct vcpu *p)
{
}

void p2m_restore_state(struct vcpu *n)
{
    struct p2m_domain *p2m = p2m_get_hostp2m(n->domain);

    if ( is_idle_vcpu(n) )
        return;

    n->arch.hgatp = p2m->hgatp;
}

mfn_t gfn_to_mfn(struct domain *d, gfn_t gfn)
{
    return p2m_lookup(d, gfn, NULL);
}

/*
 * Force a synchronous P2M TLB flush.
 *
 * Must be called with the p2m lock held.
 */
static void p2m_force_tlb_flush_sync(struct p2m_domain *p2m)
{
    asm volatile ("sfence.vma");
}

static paddr_t get_p2m_root_pt_mfn(struct domain *d)
{
    return (p2m_get_hostp2m(d)->hgatp & HGATP_PPN) << PAGE_SHIFT;
}

mfn_t p2m_lookup(struct domain *d, gfn_t gfn, p2m_type_t *t)
{
    vaddr_t root;
    paddr_t pa;
    struct p2m_domain *p2m = p2m_get_hostp2m(d);

    p2m_read_lock(p2m);
    root = (vaddr_t)map_domain_page(maddr_to_mfn(get_p2m_root_pt_mfn(d)));
    pa = pt_walk(root, gfn_to_gaddr(gfn), false);
    p2m_read_unlock(p2m);

    return maddr_to_mfn(pa);
}

mfn_t p2m_get_entry(struct p2m_domain *p2m, gfn_t gfn,
                    p2m_type_t *t, p2m_access_t *a,
                    unsigned int *page_order,
                    bool *valid)
{
    BUG();
    return (mfn_t)INVALID_MFN;

}

static inline int p2m_insert_mapping(struct domain *d,
                                     gfn_t start_gfn,
                                     unsigned long nr,
                                     mfn_t mfn,
                                     p2m_type_t t)
{
    UNIMPLEMENTED();
    return -1;
}

static inline int p2m_remove_mapping(struct domain *d,
                                     gfn_t start_gfn,
                                     unsigned long nr,
                                     mfn_t mfn)
{
    UNIMPLEMENTED();
    return 0;
}

void p2m_tlb_flush_sync(struct p2m_domain *p2m)
{
    p2m_force_tlb_flush_sync(p2m);
}

int map_regions_p2mt(struct domain *d,
                     gfn_t gfn,
                     unsigned long nr,
                     mfn_t mfn,
                     p2m_type_t p2mt)
{
    return p2m_insert_mapping(d, gfn, nr, mfn, p2mt);
}

int unmap_regions_p2mt(struct domain *d,
                       gfn_t gfn,
                       unsigned long nr,
                       mfn_t mfn)
{
    return p2m_remove_mapping(d, gfn, nr, mfn);
}

int map_mmio_regions(struct domain *d,
                     gfn_t start_gfn,
                     unsigned long nr,
                     mfn_t mfn)
{
    return p2m_insert_mapping(d, start_gfn, nr, mfn, p2m_mmio_direct_dev);
}

int unmap_mmio_regions(struct domain *d,
                       gfn_t start_gfn,
                       unsigned long nr,
                       mfn_t mfn)
{
    return p2m_remove_mapping(d, start_gfn, nr, mfn);
}

int map_dev_mmio_region(struct domain *d,
                        gfn_t gfn,
                        unsigned long nr,
                        mfn_t mfn)
{
    /* TODO */

    return 0;
}

void clear_and_clean_page(struct page_info *page)
{
    void *p = __map_domain_page(page);

    clear_page(p);
    unmap_domain_page(p);
}

static struct page_info *p2m_get_clean_page(struct domain *d)
{
    struct page_info *page;

    page = alloc_domheap_pages(NULL, 1, 0);
    if ( page == NULL )
        return NULL;

    clear_and_clean_page(page);

    return page;
}

paddr_t p2m_get_page(void *data)
{
    struct domain *d = data;
    struct page_info *page;

    page = p2m_get_clean_page(d);

    if ( !page )
        return 0;

    return page_to_maddr(page);
}

int guest_physmap_add_entry(struct domain *d,
                            gfn_t gfn,
                            mfn_t mfn,
                            unsigned long page_order,
                            p2m_type_t t)
{
    const unsigned long nr = 1 << page_order;
    paddr_t guest_start, guest_end;
    vaddr_t root;
    unsigned long i = 0;

    printk("%s: map %lu pages from gfn 0x%02lx to mfn 0x%02lx\n", __func__,
            nr, gfn_to_gaddr(gfn), mfn_to_maddr(mfn));

    root = (vaddr_t)map_domain_page(maddr_to_mfn(get_p2m_root_pt_mfn(d)));
    guest_start = gfn_to_gaddr(gfn);
    guest_end = guest_start + (nr * PAGE_SIZE);

    for (i = 0; i < nr; i++ )
    {
        paddr_t guest_addr = guest_start + (i * PAGE_SIZE);
        paddr_t supervisor_addr = mfn_to_maddr(mfn) + (i * PAGE_SIZE);
        pt_update(root, guest_addr, supervisor_addr, false,
                  d, PTE_READABLE | PTE_WRITABLE | PTE_EXECUTABLE | PTE_USER);

        /* Remove this after pt_update/pt_walk stand the test of time */
        BUG_ON(pt_walk(root, guest_addr, false) != supervisor_addr);

    }
    unmap_domain_page(root);

    return 0;
}

int guest_physmap_remove_page(struct domain *d, gfn_t gfn, mfn_t mfn,
                              unsigned int page_order)
{
    return p2m_remove_mapping(d, gfn, (1 << page_order), mfn);
}

struct page_info *p2m_get_page_from_gfn(struct domain *d, gfn_t gfn,
                                        p2m_type_t *t)
{
    p2m_type_t p2mt = {0};
    mfn_t mfn = p2m_lookup(d, gfn, &p2mt);

    if ( t )
        *t = p2mt;

    if ( !mfn_valid(mfn) )
        return NULL;

    /* TODO: use get_page */
    return mfn_to_page(mfn);
}

void vcpu_mark_events_pending(struct vcpu *v)
{
    /* TODO */
}

void vcpu_update_evtchn_irq(struct vcpu *v)
{
    /* TODO */
}

static struct page_info *p2m_allocate_root(struct domain *d)
{
    /* TODO: this must actually be 16KB aligned (not just 4K) */
    return p2m_get_clean_page(d);
}

static unsigned long hgatp_from_page_info(struct page_info *page_info)
{
    unsigned long ppn;

    ppn = (page_to_maddr(page_info) >> PAGE_SHIFT) & HGATP_PPN;

    /* ASID not supported yet */

    return ppn | (HGATP_MODE_SV39X4 << HGATP_MODE_SHIFT);
}

static int p2m_alloc_table(struct domain *d)
{
    struct p2m_domain *p2m = p2m_get_hostp2m(d);

    p2m->root = p2m_allocate_root(d);
    if ( !p2m->root )
        return -ENOMEM;

    p2m->hgatp = hgatp_from_page_info(p2m->root);

    p2m_write_lock(p2m);
    p2m_force_tlb_flush_sync(p2m);
    p2m_write_unlock(p2m);

    return 0;
}

int p2m_init(struct domain *d)
{
    struct p2m_domain *p2m = p2m_get_hostp2m(d);
    int rc;

    rc = p2m_alloc_table(d);
    if ( rc )
        return rc;

    rwlock_init(&p2m->lock);
    INIT_PAGE_LIST_HEAD(&p2m->pages);

    return 0;
}

struct page_info *get_page_from_gfn(struct domain *d, unsigned long gfn,
                                    p2m_type_t *t, p2m_query_t q)
{

    p2m_type_t _t;
    mfn_t mfn;
    struct page_info *page;

    if ( !t )
        t = &_t;

    *t = p2m_invalid;

    /* For Dom0, gfn == mfn */
    mfn = _mfn(gfn);
    page = mfn_to_page(mfn);

    /* TODO: use get/put_page for reference counting here */
    if ( !mfn_valid(mfn) )
        return NULL;

    /* TODO: actually set this based on the properties of the page */
    *t = p2m_ram_rw;

    return page;
}

int set_foreign_p2m_entry(struct domain *d, const struct domain *fd,
                          unsigned long gfn, mfn_t mfn)
{
    return -EOPNOTSUPP;
}

unsigned long p2m_pod_decrease_reservation(struct domain *d, gfn_t gfn,
                                           unsigned int order)
{
    return 0;
}
