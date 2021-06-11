/******************************************************************************
 *
 * Copyright 2019 (C) Alistair Francis <alistair.francis@wdc.com>
 * Copyright 2021 (C) Bobby Eshleman <bobby.eshleman@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _ASM_RISCV_PROCESSOR_H
#define _ASM_RISCV_PROCESSOR_H

#include <asm/types.h>

#define RISCV_CPU_USER_REGS_zero		0
#define RISCV_CPU_USER_REGS_ra		    1
#define RISCV_CPU_USER_REGS_sp		    2
#define RISCV_CPU_USER_REGS_gp		    3
#define RISCV_CPU_USER_REGS_tp		    4
#define RISCV_CPU_USER_REGS_t0		    5
#define RISCV_CPU_USER_REGS_t1		    6
#define RISCV_CPU_USER_REGS_t2		    7
#define RISCV_CPU_USER_REGS_s0		    8
#define RISCV_CPU_USER_REGS_s1		    9
#define RISCV_CPU_USER_REGS_a0		    10
#define RISCV_CPU_USER_REGS_a1		    11
#define RISCV_CPU_USER_REGS_a2		    12
#define RISCV_CPU_USER_REGS_a3		    13
#define RISCV_CPU_USER_REGS_a4		    14
#define RISCV_CPU_USER_REGS_a5		    15
#define RISCV_CPU_USER_REGS_a6		    16
#define RISCV_CPU_USER_REGS_a7		    17
#define RISCV_CPU_USER_REGS_s2		    18
#define RISCV_CPU_USER_REGS_s3		    19
#define RISCV_CPU_USER_REGS_s4		    20
#define RISCV_CPU_USER_REGS_s5		    21
#define RISCV_CPU_USER_REGS_s6		    22
#define RISCV_CPU_USER_REGS_s7		    23
#define RISCV_CPU_USER_REGS_s8		    24
#define RISCV_CPU_USER_REGS_s9		    25
#define RISCV_CPU_USER_REGS_s10		    26
#define RISCV_CPU_USER_REGS_s11		    27
#define RISCV_CPU_USER_REGS_t3		    28 
#define RISCV_CPU_USER_REGS_t4		    29
#define RISCV_CPU_USER_REGS_t5		    30
#define RISCV_CPU_USER_REGS_t6		    31
#define RISCV_CPU_USER_REGS_sepc		32
#define RISCV_CPU_USER_REGS_sstatus	    33 
#define RISCV_CPU_USER_REGS_hstatus	    34
#define RISCV_CPU_USER_REGS_sp_exec		35
#define RISCV_CPU_USER_REGS_last		36

#define RISCV_CPU_USER_REGS_OFFSET(x)	((RISCV_CPU_USER_REGS_##x) * __SIZEOF_POINTER__)
#define RISCV_CPU_USER_REGS_SIZE		RISCV_CPU_USER_REGS_OFFSET(last)

#define RISCV_PCPUINFO_processor_id     0
#define RISCV_PCPUINFO_cpu_info         1
#define RISCV_PCPUINFO_tmp              2
#define RISCV_PCPUINFO_last             3
#define RISCV_PCPUINFO_OFFSET(x)	((RISCV_PCPUINFO_##x) * __SIZEOF_POINTER__)
#define RISCV_PCPUINFO_SIZE		    RISCV_PCPUINFO_OFFSET(last)

#ifndef __ASSEMBLY__

register struct pcpu_info *tp asm ("tp");

struct pcpu_info {
    unsigned long processor_id;
    struct cpu_info *cpu_info;

    /* temporary variable to be used during save/restore of vcpu regs */
    unsigned long tmp;
};

/* tp points to one of these */
extern struct pcpu_info pcpu_info[NR_CPUS];

#define get_processor_id()    (tp->processor_id)
#define set_processor_id(id)  do {                          \
    tp->processor_id = id;                            \
} while(0)

/* On stack VCPU state */
struct cpu_user_regs
{
	register_t zero;
	register_t ra;
	register_t sp;
	register_t gp;
	register_t tp;
	register_t t0;
	register_t t1;
	register_t t2;
	register_t s0;
	register_t s1;
	register_t a0;
	register_t a1;
	register_t a2;
	register_t a3;
	register_t a4;
	register_t a5;
	register_t a6;
	register_t a7;
	register_t s2;
	register_t s3;
	register_t s4;
	register_t s5;
	register_t s6;
	register_t s7;
	register_t s8;
	register_t s9;
	register_t s10;
	register_t s11;
	register_t t3;
	register_t t4;
	register_t t5;
	register_t t6;
	register_t sepc;
	register_t sstatus;
	register_t hstatus;
	register_t sp_exec;
};

void show_execution_state(const struct cpu_user_regs *regs);
void show_registers(const struct cpu_user_regs *regs);

/* All a bit UP for the moment */
#define cpu_to_core(_cpu)   (0)
#define cpu_to_socket(_cpu) (0)

/* Based on Linux: arch/riscv/include/asm/processor.h */

static inline void cpu_relax(void)
{
	int dummy;
	/* In lieu of a halt instruction, induce a long-latency stall. */
	__asm__ __volatile__ ("div %0, %0, zero" : "=r" (dummy));
	barrier();
}

static inline void wait_for_interrupt(void)
{
	__asm__ __volatile__ ("wfi");
}

#endif /* __ASSEMBLY__ */

#endif /* _ASM_RISCV_PROCESSOR_H */
