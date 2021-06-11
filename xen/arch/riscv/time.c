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
#include <asm/system.h>
#include <asm/time.h>

#define QEMU_TIMEBASE_FREQ 0x989680

unsigned long __read_mostly cpu_khz;  /* CPU clock frequency in kHz. */

uint64_t __read_mostly boot_count;

s_time_t ticks_to_ns(uint64_t ticks)
{
    return muldiv64(ticks, SECONDS(1), 1000 * cpu_khz);
}

void __init preinit_xen_time(void)
{
    /* TODO: get from DT cpus { timebase-frequency } */
    cpu_khz = QEMU_TIMEBASE_FREQ / 1000;
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
    /* TODO */
    return 1;
}
