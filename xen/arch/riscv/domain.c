#include <xen/errno.h>
#include <xen/init.h>
#include <xen/lib.h>
#include <xen/sched.h>
#include <xen/domain.h>
#include <xen/softirq.h>
#include <asm/vtimer.h>
#include <asm/traps.h>
#include <public/domctl.h>
#include <public/xen.h>

DEFINE_PER_CPU(struct vcpu *, curr_vcpu);

struct vcpu *alloc_dom0_vcpu0(struct domain *dom0)
{
    return vcpu_create(dom0, 0);
}

void context_switch(struct vcpu *prev, struct vcpu *next)
{
    ASSERT(prev != next);

    local_irq_disable();
    set_current(next);

    p2m_save_state(prev);
    p2m_restore_state(next);

    vtimer_save(prev);
    vtimer_restore(next);

    ASSERT(is_idle_vcpu(prev) || trap_from_guest);
    tp->guest_cpu_info = next->arch.cpu_info;
    tp->stack_cpu_regs = &next->arch.cpu_info->guest_cpu_user_regs;

    /* __handle_exception handles CSRs */

    /* TODO Handle floating point registers */
    prev = __context_switch(prev, next);


    local_irq_enable();
    sched_context_switched(prev, current);
}

static void do_idle(void)
{
    unsigned int cpu = smp_processor_id();

    rcu_idle_enter(cpu);
    /* rcu_idle_enter() can raise TIMER_SOFTIRQ. Process it now. */
    process_pending_softirqs();

    local_irq_disable();
    if ( cpu_is_haltable(cpu) )
    {
        wait_for_interrupt();
    }
    local_irq_enable();

    rcu_idle_exit(cpu);
}

void idle_loop(void)
{
    unsigned int cpu = smp_processor_id();

    printk("%s\n", __func__);

    for ( ; ; )
    {
        if ( unlikely(tasklet_work_to_do(cpu)) )
            do_tasklet();
        else if ( !softirq_pending(cpu) && !scrub_free_pages() &&
                  !softirq_pending(cpu) )
            do_idle();

        do_softirq();
    }
}

void noreturn startup_cpu_idle_loop(void)
{
    struct vcpu *v = current;

    ASSERT(is_idle_vcpu(v));

    reset_stack_and_jump(idle_loop);

    /* This function is noreturn */
    BUG();
}

void continue_running(struct vcpu *same)
{
    /* TODO */
}

void sync_local_execstate(void)
{
    /* TODO */
}

void sync_vcpu_execstate(struct vcpu *v)
{
    /* TODO */
}

unsigned long hypercall_create_continuation(
    unsigned int op, const char *format, ...)
{
	/* TODO */

	return 0;
}

struct domain *alloc_domain_struct(void)
{
    struct domain *d;
    BUILD_BUG_ON(sizeof(*d) > PAGE_SIZE);
    d = alloc_xenheap_pages(0, 0);
    if ( d == NULL )
        return NULL;

    clear_page(d);
    return d;
}

void free_domain_struct(struct domain *d)
{
    /* TODO */
}

void dump_pageframe_info(struct domain *d)
{
    /* TODO */
}

int arch_sanitise_domain_config(struct xen_domctl_createdomain *config)
{
    return 0;
}


int arch_domain_create(struct domain *d,
                       struct xen_domctl_createdomain *config)
{
    int rc = 0;

    if ( is_idle_domain(d) )
        return 0;

    if ( (rc = p2m_init(d)) != 0)
        goto fail;

    if ( (rc = domain_vtimer_init(d, &config->arch)) != 0 )
        goto fail;

    return rc;
    
fail:
    d->is_dying = DOMDYING_dead;
    arch_domain_destroy(d);
    return rc;
}

void arch_domain_destroy(struct domain *d)
{
    /* TODO */
}

void arch_domain_shutdown(struct domain *d)
{
    /* TODO */
}

void arch_domain_pause(struct domain *d)
{
    /* TODO */
}

void arch_domain_unpause(struct domain *d)
{
    /* TODO */
}

int arch_domain_soft_reset(struct domain *d)
{
    /* TODO */
    return -ENOSYS;
}

void arch_domain_creation_finished(struct domain *d)
{
    /* TODO */
}

int domain_relinquish_resources(struct domain *d)
{
    /* TODO */
    return -ENOSYS;
}

void arch_dump_domain_info(struct domain *d)
{
    /* TODO */
}

long arch_do_vcpu_op(int cmd, struct vcpu *v, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    /* TODO */
    return -ENOSYS;
}

void arch_dump_vcpu_info(struct vcpu *v)
{
    /* TODO */
}

int arch_set_info_guest(
    struct vcpu *v, vcpu_guest_context_u c)
{
    /* TODO */
    return -ENOSYS;
}

#define MAX_PAGES_PER_VCPU 1

/* taken from arm/domain.c */
struct vcpu *alloc_vcpu_struct(const struct domain *d)
{
    struct vcpu *v;
    unsigned int i;

    BUILD_BUG_ON(sizeof(*v) > MAX_PAGES_PER_VCPU * PAGE_SIZE);
    v = alloc_xenheap_pages(get_order_from_bytes(sizeof(*v)), 0);

    if ( v == NULL )
        return v;

    for ( i = 0; i < DIV_ROUND_UP(sizeof(*v), PAGE_SIZE); i++ )
        clear_page((void *)v + i * PAGE_SIZE);

    return v;
}

void free_vcpu_struct(struct vcpu *v)
{
    /* TODO */
}

int arch_initialise_vcpu(struct vcpu *v, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    /* TODO */
    return -ENOSYS;
}

int arch_vcpu_reset(struct vcpu *v)
{
    /* TODO */
    return -ENOSYS;
}

static void continue_new_vcpu(void)
{
    reset_stack_and_jump(return_to_new_vcpu64);
}

static void vcpu_csr_init(struct vcpu *v)
{
    unsigned long hedeleg, hstatus;

    hedeleg = 0;
    hedeleg |= (1U << CAUSE_MISALIGNED_FETCH);
    hedeleg |= (1U << CAUSE_FETCH_ACCESS);
    hedeleg |= (1U << CAUSE_ILLEGAL_INSTRUCTION);
    hedeleg |= (1U << CAUSE_MISALIGNED_LOAD);
    hedeleg |= (1U << CAUSE_LOAD_ACCESS);
    hedeleg |= (1U << CAUSE_MISALIGNED_STORE);
    hedeleg |= (1U << CAUSE_STORE_ACCESS);
    hedeleg |= (1U << CAUSE_BREAKPOINT);
    hedeleg |= (1U << CAUSE_USER_ECALL);
    hedeleg |= (1U << CAUSE_FETCH_PAGE_FAULT);
    hedeleg |= (1U << CAUSE_LOAD_PAGE_FAULT);
    hedeleg |= (1U << CAUSE_STORE_PAGE_FAULT);
    v->arch.hedeleg = hedeleg;

    hstatus = HSTATUS_SPV | HSTATUS_SPVP;
    v->arch.hstatus = hstatus;

    /* Enable all timers for guest */
    v->arch.hcounteren = -1UL;

    /* Enable floating point and other extensions for guest. */
    /* TODO Disable them in Xen. */
    csr_clear(CSR_SSTATUS, SSTATUS_FS | SSTATUS_XS);
    csr_set(CSR_SSTATUS, SSTATUS_FS_INITIAL | SSTATUS_XS_INITIAL);
}

int arch_vcpu_create(struct vcpu *v)
{
    int rc = 0;

    BUILD_BUG_ON( sizeof(struct cpu_info) > STACK_SIZE );

    v->arch.stack = alloc_xenheap_pages(STACK_ORDER, MEMF_node(vcpu_to_node(v)));
    if ( v->arch.stack == NULL )
        return -ENOMEM;

    v->arch.cpu_info = (struct cpu_info *)(v->arch.stack
                                           + STACK_SIZE
                                           - sizeof(struct cpu_info));

    /* Back reference to vcpu is used to access its processor field */
    memset(v->arch.cpu_info, 0, sizeof(*v->arch.cpu_info));

    v->arch.saved_context.sp = (register_t)v->arch.cpu_info;
    v->arch.saved_context.ra = (register_t)continue_new_vcpu;

    printk(XENLOG_INFO "Create vCPU with sp=0x%02lx, pc=0x%02lx\n",
            v->arch.saved_context.sp, v->arch.saved_context.ra);

    v->arch.vplic = vplic_alloc();

    if ( !v->arch.vplic )
    {
        free_xenheap_pages(v->arch.stack, STACK_ORDER);
        return -ENOMEM;
    }

    vcpu_csr_init(v);

    if ( (rc = vcpu_vtimer_init(v)) != 0 )
        goto fail;

    return rc;

 fail:
    arch_vcpu_destroy(v);
    return rc;
}

void arch_vcpu_destroy(struct vcpu *v)
{
    /* TODO */
}
