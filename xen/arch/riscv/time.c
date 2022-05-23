/*
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

#include <xen/console.h>
#include <xen/init.h>
#include <xen/irq.h>
#include <xen/lib.h>
#include <xen/mm.h>
#include <xen/softirq.h>
#include <xen/sched.h>
#include <xen/time.h>
#include <xen/sched.h>
#include <xen/event.h>
#include <xen/cpu.h>
#include <xen/notifier.h>
#include <asm/acpi.h>
#include <asm/system.h>
#include <asm/time.h>
#include <asm/sbi.h>

unsigned long __read_mostly cpu_khz;  /* CPU clock frequency in kHz. */

uint32_t __read_mostly timer_dt_clock_frequency;

uint64_t __read_mostly boot_count;

static inline s_time_t ticks_to_ns(uint64_t ticks)
{
    return muldiv64(ticks, SECONDS(1), 1000 * cpu_khz);
}

static inline uint64_t ns_to_ticks(s_time_t ns)
{
    return muldiv64(ns, 1000 * cpu_khz, SECONDS(1));
}

/* Set up the timer on the boot CPU (early init function) */
static void __init preinit_dt_xen_time(void)
{
    static const struct dt_device_match dt_cpus[] __initconst =
    {
        DT_MATCH_PATH("/cpus"),
        { /* sentinel */ },
    };
    int res;
    u32 rate;
    struct dt_device_node *cpus_node;

    cpus_node = dt_find_matching_node(NULL, dt_cpus);
    if ( !cpus_node )
        panic("No cpus node in device tree.\n");

    res = dt_property_read_u32(cpus_node, "timebase-frequency", &rate);
    if ( !res )
        panic("Unable to find clock frequency.\n");
    cpu_khz = rate / 1000;
    timer_dt_clock_frequency = rate;
}

void __init preinit_xen_time(void)
{
    if ( acpi_disabled )
        preinit_dt_xen_time();
    else
        panic("TODO ACPI\n");

    boot_count = get_cycles();
}

/* Set up the timer on the boot CPU (late init function) */
int __init init_xen_time(void)
{

    /* TODO */
    return 0;
}

s_time_t get_s_time(void)
{
    uint64_t ticks = get_cycles() - boot_count;
    return ticks_to_ns(ticks);
}


/* VCPU PV timers. */
void send_timer_event(struct vcpu *v)
{
    /* TODO */
    BUG();
}

/* VCPU PV clock. */
void update_vcpu_system_time(struct vcpu *v)
{
    /* TODO */
    BUG();
}

void domain_set_time_offset(struct domain *d, int64_t time_offset_seconds)
{
    /* TODO */
    BUG();
}

int reprogram_timer(s_time_t timeout)
{
    uint64_t deadline, now;

    if (timeout == 0)
    {
        /* Disable timers */
        csr_clear(CSR_SIE, 1ul << IRQ_S_TIMER);
        return 1;
    }
    
    deadline = ns_to_ticks(timeout) + boot_count;
    now = get_cycles();
    if (deadline <= now)
        return 0;

    /* Enable timer */
    sbi_set_timer(deadline);
    csr_set(CSR_SIE, 1ul << IRQ_S_TIMER);

    return 1;
}

void timer_interrupt(unsigned long cause, struct cpu_user_regs *regs)
{
    /* Disable the timer to avoid more interrupts */
    csr_clear(CSR_SIE, 1ul << IRQ_S_TIMER);

    /* Signal the generic timer code to do its work */
    raise_softirq(TIMER_SOFTIRQ);
}

/* Set up the timer interrupt on this CPU */
void init_timer_interrupt(void)
{
}
