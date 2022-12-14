 /*
 * Copyright (C) 2012 Regents of the University of California
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#ifndef _ASM_RISCV_TIMEX_H
#define _ASM_RISCV_TIMEX_H

#include <asm/processor.h>
#include <xen/lib.h>

typedef unsigned long cycles_t;

static inline s_time_t ticks_to_ns(uint64_t ticks)
{
    return muldiv64(ticks, SECONDS(1), 1000 * cpu_khz);
}

static inline uint64_t ns_to_ticks(s_time_t ns)
{
    return muldiv64(ns, 1000 * cpu_khz, SECONDS(1));
}

static inline cycles_t get_cycles_inline(void)
{
	cycles_t n;

	__asm__ __volatile__ (
		"rdtime %0"
		: "=r" (n));
	return n;
}
#define get_cycles get_cycles_inline

#ifdef CONFIG_64BIT
static inline uint64_t get_cycles64(void)
{
        return get_cycles();
}
#else
static inline uint64_t get_cycles64(void)
{
	u32 lo, hi, tmp;
	__asm__ __volatile__ (
		"1:\n"
		"rdtimeh %0\n"
		"rdtime %1\n"
		"rdtimeh %2\n"
		"bne %0, %2, 1b"
		: "=&r" (hi), "=&r" (lo), "=&r" (tmp));
	return ((u64)hi << 32) | lo;
}
#endif

#define ARCH_HAS_READ_CURRENT_TIMER

static inline int read_current_timer(unsigned long *timer_val)
{
	*timer_val = get_cycles();
	return 0;
}

extern void preinit_xen_time(void);

extern void init_timer_interrupt(void);

extern void timer_interrupt(unsigned long cause, struct cpu_user_regs *regs);

extern void force_update_vcpu_system_time(struct vcpu *v);

extern uint64_t boot_count;

#endif /* _ASM_RISCV_TIMEX_H */
