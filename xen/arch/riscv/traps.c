/*
 * RISC-V Trap handlers
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

#include <xen/domain_page.h>
#include <xen/const.h>
#include <xen/errno.h>
#include <xen/hypercall.h>
#include <xen/init.h>
#include <xen/iocap.h>
#include <xen/irq.h>
#include <xen/lib.h>
#include <xen/livepatch.h>
#include <xen/mem_access.h>
#include <xen/mm.h>
#include <xen/perfc.h>
#include <xen/smp.h>
#include <xen/softirq.h>
#include <xen/string.h>
#include <xen/symbols.h>
#include <xen/version.h>
#include <xen/virtual_region.h>

#include <asm/sbi.h>
#include <asm/traps.h>
#include <asm/guest_access.h>
#include <asm/vtimer.h>

/* Included just for hardcoded values during development */
#include <asm/setup.h>

#include <public/sched.h>
#include <public/xen.h>

#define INSN_16BIT_MASK 0x3

#define print_csr(_csr) \
    do {    \
        printk("\t" #_csr ": 0x%02lx\n", csr_read(_csr)); \
    } while ( 0 )

static inline void advance_pc(struct cpu_user_regs *regs)
{
    regs->sepc += 4;
}

const char *decode_trap_cause(unsigned long cause)
{
    switch ( cause )
    {
    case CAUSE_MISALIGNED_FETCH:
        return "Instruction Address Misaligned";
    case CAUSE_FETCH_ACCESS:
        return "Instruction Access Fault";
    case CAUSE_ILLEGAL_INSTRUCTION:
        return "Illegal Instruction";
    case CAUSE_BREAKPOINT:
        return "Breakpoint";
    case CAUSE_MISALIGNED_LOAD:
        return "Load Address Misaligned";
    case CAUSE_LOAD_ACCESS:
        return "Load Access Fault";
    case CAUSE_MISALIGNED_STORE:
        return "Store/AMO Address Misaligned";
    case CAUSE_STORE_ACCESS:
        return "Store/AMO Access Fault";
    case CAUSE_USER_ECALL:
        return "Environment Call from U-Mode";
    case CAUSE_SUPERVISOR_ECALL:
        return "Environment Call from S-Mode";
    case CAUSE_MACHINE_ECALL:
        return "Environment Call from M-Mode";
    case CAUSE_FETCH_PAGE_FAULT:
        return "Instruction Page Fault";
    case CAUSE_LOAD_PAGE_FAULT:
        return "Load Page Fault";
    case CAUSE_STORE_PAGE_FAULT:
        return "Store/AMO Page Fault";
    case CAUSE_FETCH_GUEST_PAGE_FAULT:
        return "Instruction Guest Page Fault";
    case CAUSE_LOAD_GUEST_PAGE_FAULT:
        return "Load Guest Page Fault";
    case CAUSE_VIRTUAL_INST_FAULT:
        return "Virtualized Instruction Fault";
    case CAUSE_STORE_GUEST_PAGE_FAULT:
        return "Guest Store/AMO Page Fault";
    default:
        return "UNKNOWN";
    }
}

const char *decode_reserved_interrupt_cause(unsigned long irq_cause)
{
    switch ( irq_cause )
    {
    case IRQ_M_SOFT:
        return "M-mode Software Interrupt";
    case IRQ_M_TIMER:
        return "M-mode TIMER Interrupt";
    case IRQ_M_EXT:
        return "M-mode TIMER Interrupt";
    default:
        return "UNKNOWN IRQ type";
    }
}

const char *decode_interrupt_cause(unsigned long cause)
{
    unsigned long irq_cause = cause & ~CAUSE_IRQ_FLAG;

    switch ( irq_cause )
    {
    case IRQ_S_SOFT:
        return "Supervisor Software Interrupt";
    case IRQ_S_TIMER:
        return "Supervisor Timer Interrupt";
    case IRQ_S_EXT:
        return "Supervisor External Interrupt";
    default:
        return decode_reserved_interrupt_cause(irq_cause);
    }
}

const char *decode_cause(unsigned long cause)
{
    if ( cause & CAUSE_IRQ_FLAG )
        return decode_interrupt_cause(cause);

    return decode_trap_cause(cause);
}

static void dump_csrs(unsigned long cause)
{
    unsigned long hstatus;
    bool gva;

    printk("\nUnhandled Exception! dumping CSRs...\n");

    printk("Supervisor CSRs\n");
    print_csr(CSR_STVEC);
    print_csr(CSR_SATP);
    print_csr(CSR_SEPC);

    hstatus = csr_read(CSR_HSTATUS);
    gva = !!(hstatus & HSTATUS_GVA);

    printk("\tCSR_STVAL: 0x%02lx%s\n",
            csr_read(CSR_STVAL),
            gva ? ", (guest virtual address)" : "");

    printk("\tCSR_SCAUSE: 0x%02lx\n", cause);
    printk("\t\tDescription: %s\n", decode_cause(cause));
    print_csr(CSR_SSTATUS);

    printk("\nVirtual Supervisor CSRs\n");
    print_csr(CSR_VSTVEC);
    print_csr(CSR_VSATP);
    print_csr(CSR_VSEPC);
    print_csr(CSR_VSTVAL);
    cause = csr_read(CSR_VSCAUSE);
    printk("\tCSR_VCAUSE: 0x%02lx\n", cause);
    printk("\t\tDescription: %s\n", decode_cause(cause));
    print_csr(CSR_VSSTATUS);

    printk("\nHypervisor CSRs\n");

    print_csr(CSR_HSTATUS);
    printk("\t\thstatus.VTSR=%d\n", !!(hstatus & HSTATUS_VTSR));
    printk("\t\thstatus.VTVM=%d\n", !!(hstatus & HSTATUS_VTVM));
    printk("\t\thstatus.HU=%d\n", !!(hstatus & HSTATUS_HU));
    printk("\t\thstatus.SPVP=%d\n", !!(hstatus & HSTATUS_SPVP));
    printk("\t\thstatus.SPV=%d\n", !!(hstatus & HSTATUS_SPV));
    printk("\t\thstatus.GVA=%d\n", !!(hstatus & HSTATUS_GVA));
    print_csr(CSR_HGATP);
    print_csr(CSR_HTVAL);
    print_csr(CSR_HTINST);
    print_csr(CSR_HEDELEG);
    print_csr(CSR_HIDELEG);

    for (;;)
        wait_for_interrupt();
}

static void guest_sbi_set_timer(struct cpu_user_regs *regs)
{
    struct vcpu *v = current;
    vtimer_set_timer(&v->arch.vtimer, regs->a0);
    regs->a0 = 0;
}

static void guest_sbi_putchar(struct cpu_user_regs *regs)
{
    sbi_console_putchar((int)regs->a0);
    regs->a0 = 0;
}

static void handle_guest_sbi(struct cpu_user_regs *regs)
{
    unsigned long eid = regs->a7;

    switch ( eid )
    {
    case SBI_EXT_0_1_SET_TIMER:
        guest_sbi_set_timer(regs);
        break;
    case SBI_EXT_0_1_CONSOLE_PUTCHAR:
        guest_sbi_putchar(regs);
        break;
    case SBI_EXT_0_1_CONSOLE_GETCHAR:
        printk("%s:%d: unimplemented: SBI_EXT_0_1_CONSOLE_GETCHAR\n",
               __FILE__, __LINE__);
        regs->a0 = SBI_ERR_NOT_SUPPORTED;
        break;
    case SBI_EXT_0_1_CLEAR_IPI:
        printk("%s:%d: unimplemented: SBI_EXT_0_1_CLEAR_IPI\n",
               __FILE__, __LINE__);
        regs->a0 = SBI_ERR_NOT_SUPPORTED;
        break;
    case SBI_EXT_0_1_SEND_IPI:
        printk("%s:%d: unimplemented: SBI_EXT_0_1_SEND_IPI\n",
               __FILE__, __LINE__);
        regs->a0 = SBI_ERR_NOT_SUPPORTED;
        break;
    case SBI_EXT_0_1_SHUTDOWN:
        printk("%s:%d: unimplemented: SBI_EXT_0_1_SHUTDOWN\n",
               __FILE__, __LINE__);
        regs->a0 = SBI_ERR_NOT_SUPPORTED;
        break;
    case SBI_EXT_0_1_REMOTE_FENCE_I:
        printk("%s:%d: unimplemented: SBI_EXT_0_1_REMOTE_FENCE_I\n",
               __FILE__, __LINE__);
        regs->a0 = SBI_ERR_NOT_SUPPORTED;
        break;
    case SBI_EXT_0_1_REMOTE_SFENCE_VMA:
        printk("%s:%d: unimplemented: SBI_EXT_0_1_REMOTE_SFENCE_VMA\n",
               __FILE__, __LINE__);
        regs->a0 = SBI_ERR_NOT_SUPPORTED;
        break;
    case SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID:
        printk("%s:%d: unimplemented: SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID\n",
               __FILE__, __LINE__);
        regs->a0 = SBI_ERR_NOT_SUPPORTED;
        break;
    case SBI_EXT_BASE:
        regs->a0 = SBI_ERR_NOT_SUPPORTED;
        break;
    default:
        printk("UNKNOWN Guest SBI extension id 0x%lx\n", eid);
        regs->a0 = SBI_ERR_NOT_SUPPORTED;
        break;
    };

    advance_pc(regs);
}

static inline unsigned long get_faulting_gpa(void)
{
    return (csr_read(CSR_HTVAL) << 2) | (csr_read(CSR_STVAL) & 0x3);
}

static bool is_plic_access(unsigned long addr)
{
    return PLIC_BASE < addr && addr < PLIC_END;
}

static int emulate_load(struct vcpu *vcpu, unsigned long fault_addr,
                        unsigned long htinst)
{
    uint8_t data8;
    uint16_t data16;
    uint32_t data32;
    uint64_t data64;
    int rc;
	unsigned long insn;
	int shift = 0, len = 0, insn_len = 0;
	struct riscv_trap utrap = { 0 };

	/* Determine trapped instruction */
	if (htinst & 0x1) {
		/*
		 * Bit[0] == 1 implies trapped instruction value is
		 * transformed instruction or custom instruction.
		 */
		insn = htinst | INSN_16BIT_MASK;
		insn_len = (htinst & BIT(1, UL)) ? INSN_LEN(insn) : 2;
	} else {
		/*
		 * Bit[0] == 0 implies trapped instruction value is
		 * zero or special value.
		 */
		insn = riscv_vcpu_unpriv_read(vcpu, true, guest_regs(vcpu)->sepc,
                                      &utrap);
		if (utrap.scause) {
			/* Redirect trap if we failed to read instruction */
			utrap.sepc = guest_regs(vcpu)->sepc;
            printk("TODO: we failed to read the trapped insns, "
                   "so redirect trap to guest\n");
			return 1;
		}
		insn_len = INSN_LEN(insn);
	}

	/* Decode length of MMIO and shift */
	if ((insn & INSN_MASK_LW) == INSN_MATCH_LW) {
		len = 4;
		shift = 8 * (sizeof(unsigned long) - len);
	} else if ((insn & INSN_MASK_LB) == INSN_MATCH_LB) {
		len = 1;
		shift = 8 * (sizeof(unsigned long) - len);
	} else if ((insn & INSN_MASK_LBU) == INSN_MATCH_LBU) {
		len = 1;
		shift = 8 * (sizeof(unsigned long) - len);
#ifdef CONFIG_64BIT
	} else if ((insn & INSN_MASK_LD) == INSN_MATCH_LD) {
		len = 8;
		shift = 8 * (sizeof(unsigned long) - len);
	} else if ((insn & INSN_MASK_LWU) == INSN_MATCH_LWU) {
		len = 4;
#endif
	} else if ((insn & INSN_MASK_LH) == INSN_MATCH_LH) {
		len = 2;
		shift = 8 * (sizeof(unsigned long) - len);
	} else if ((insn & INSN_MASK_LHU) == INSN_MATCH_LHU) {
		len = 2;
#ifdef CONFIG_64BIT
	} else if ((insn & INSN_MASK_C_LD) == INSN_MATCH_C_LD) {
		len = 8;
		shift = 8 * (sizeof(unsigned long) - len);
		insn = RVC_RS2S(insn) << SH_RD;
	} else if ((insn & INSN_MASK_C_LDSP) == INSN_MATCH_C_LDSP &&
		   ((insn >> SH_RD) & 0x1f)) {
		len = 8;
		shift = 8 * (sizeof(unsigned long) - len);
#endif
	} else if ((insn & INSN_MASK_C_LW) == INSN_MATCH_C_LW) {
		len = 4;
		shift = 8 * (sizeof(unsigned long) - len);
		insn = RVC_RS2S(insn) << SH_RD;
	} else if ((insn & INSN_MASK_C_LWSP) == INSN_MATCH_C_LWSP &&
		   ((insn >> SH_RD) & 0x1f)) {
		len = 4;
		shift = 8 * (sizeof(unsigned long) - len);
	} else {
		return -EOPNOTSUPP;
	}

	/* Fault address should be aligned to length of MMIO */
	if (fault_addr & (len - 1))
		return -EIO;

    printk("emulating load: pc=0x%02lx, addr=0x%02lx, len=%d, shift=%d\n",
            guest_regs(vcpu)->sepc, fault_addr, len, shift);

    if ( is_plic_access(fault_addr) )
    {
        switch ( len )
        {
        case 1:
            rc = vplic_emulate_load(current, fault_addr, &data8, len);
            if ( rc < 0 )
                return rc;
            SET_RD(insn, guest_regs(vcpu), ((unsigned long)data8 << shift) >> shift);
            break;
        case 2:
            rc = vplic_emulate_load(current, fault_addr, &data16, len);
            if ( rc < 0 )
                return rc;
            SET_RD(insn, guest_regs(vcpu), ((unsigned long)data16 << shift) >> shift);
            break;
        case 4:
            rc = vplic_emulate_load(current, fault_addr, &data32, len);
            if ( rc < 0 )
                return rc;
            SET_RD(insn, guest_regs(vcpu), ((unsigned long)data32 << shift) >> shift);
            break;
        case 8:
            rc = vplic_emulate_load(current, fault_addr, &data64, len);
            if ( rc < 0 )
                return rc;
            SET_RD(insn, guest_regs(vcpu), ((unsigned long)data64 << shift) >> shift);
            break;
        default:
            BUG();
        }
    }

    advance_pc(guest_regs(vcpu));

	return 0;
}

static void handle_guest_page_fault(unsigned long cause, struct cpu_user_regs *regs)
{
    unsigned long addr;

    BUG_ON(cause != CAUSE_LOAD_GUEST_PAGE_FAULT && cause != CAUSE_STORE_GUEST_PAGE_FAULT);

    addr = get_faulting_gpa();

    printk("%s: TODO: handle faulted guest IO %s @ addr 0x%02lx\n",
            __func__,
            (cause == CAUSE_LOAD_GUEST_PAGE_FAULT) ? "load" : "store",
            addr);

    if ( cause == CAUSE_LOAD_GUEST_PAGE_FAULT )
    {
        emulate_load(current, get_faulting_gpa(), csr_read(CSR_HTINST));
    }
    else if ( cause == CAUSE_STORE_GUEST_PAGE_FAULT )
    {
        printk("TODO: emulate_store()\n");
        advance_pc(regs);
    }
}

void __handle_exception(void)
{
    unsigned long cause = csr_read(CSR_SCAUSE);
    struct cpu_user_regs *regs = guest_cpu_user_regs();

    if ( cause & CAUSE_IRQ_FLAG )
    {
        /* Handle interrupt */
        unsigned long icause = cause & ~CAUSE_IRQ_FLAG;
        switch ( icause )
        {
        case IRQ_S_TIMER:
            timer_interrupt(cause, regs);
            break;
        default:
            dump_csrs(cause);
            break;
        }
    }
    else
    {
        switch ( cause )
        {
        case CAUSE_VIRTUAL_SUPERVISOR_ECALL:
            handle_guest_sbi(regs);
            break;
        case CAUSE_LOAD_GUEST_PAGE_FAULT:
        case CAUSE_STORE_GUEST_PAGE_FAULT:
            handle_guest_page_fault(cause, regs);
            break;
        default:
            dump_csrs(cause);
            break;
        }
    }
}

void show_stack(const struct cpu_user_regs *regs)
{
    /* TODO */
    BUG();
}

void show_execution_state(const struct cpu_user_regs *regs)
{
    /* TODO */
    BUG();
}

void vcpu_show_execution_state(struct vcpu *v)
{
    /* TODO */
    BUG();
}

void arch_hypercall_tasklet_result(struct vcpu *v, long res)
{
	/* TODO */
    BUG();
}

enum mc_disposition arch_do_multicall_call(struct mc_state *state)
{
    /* TODO */
    BUG();
    return mc_continue;
}

