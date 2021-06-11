#include <xen/mm.h>
#include <xen/smp.h>
#include <asm/system.h>
#include <asm/smp.h>
#include <asm/page.h>
#include <asm/flushtlb.h>

volatile unsigned long start_secondary_pen_release = HARTID_INVALID;

/* tp points to one of these per cpu */
struct pcpu_info pcpu_info[NR_CPUS];

void flush_tlb_mask(const cpumask_t *mask)
{
    /* TODO */
    BUG();
}

void smp_send_event_check_mask(const cpumask_t *mask)
{
    /* TODO */
    printk("%s: smp not supported yet\n", __func__);
}

void smp_send_call_function_mask(const cpumask_t *mask)
{
    /* TODO */
    printk("%s: smp not supported yet\n", __func__);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
