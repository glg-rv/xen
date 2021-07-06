#include <asm/asm.h>
#include <asm/asm-offsets.h>
#include <asm/config.h>
#include <asm/csr.h>
#include <asm/domain.h>
#include <asm/init.h>
#include <asm/processor.h>
#include <asm/riscv_encoding.h>
#include <asm/traps.h>

    .global handle_exception
    .align 4
handle_exception:
    /*
     * swap sscratch and tp
     *
     * After the swap, if a guest trapped, tp will have a pointer to pcpu_info of the guest.
     * After the swap, If xen trapped, tp will be empty.
     */
    csrrw   tp, CSR_SCRATCH, tp

    /* TODO: handle empty TP, i.e., traps from xen */

    /* tp->tmp = sp */
    REG_S   sp, RISCV_PCPUINFO_OFFSET(tmp)(tp)

    /* sp = ((struct pcpu_info*)tp)->cpu_info */
    REG_L   sp, RISCV_PCPUINFO_OFFSET(cpu_info)(tp)

    /* sp->cpu_info->sp = tp->tmp */
    REG_L   t0, RISCV_PCPUINFO_OFFSET(tmp)(tp)
    REG_S   t0, RISCV_CPU_USER_REGS_OFFSET(sp)(sp)

    /* Save registers */
    REG_S   a0, RISCV_CPU_USER_REGS_OFFSET(a0)(sp)
    REG_S   a1, RISCV_CPU_USER_REGS_OFFSET(a1)(sp)
    REG_S   a2, RISCV_CPU_USER_REGS_OFFSET(a2)(sp)
    REG_S   a3, RISCV_CPU_USER_REGS_OFFSET(a3)(sp)
    REG_S   a4, RISCV_CPU_USER_REGS_OFFSET(a4)(sp)
    REG_S   a5, RISCV_CPU_USER_REGS_OFFSET(a5)(sp)
    REG_S   a6, RISCV_CPU_USER_REGS_OFFSET(a6)(sp)
    REG_S   a7, RISCV_CPU_USER_REGS_OFFSET(a7)(sp)

    /* Save return address register */
    REG_S   ra, RISCV_CPU_USER_REGS_OFFSET(ra)(sp)

    /* Save sepc */
    csrr    t0, sepc
    REG_S   t0, RISCV_CPU_USER_REGS_OFFSET(sepc)(sp)

    /* Save hstatus */
    csrr    t0, hstatus
    REG_S   t0, RISCV_CPU_USER_REGS_OFFSET(hstatus)(sp)

    jal __handle_exception

    REG_L   sp, RISCV_PCPUINFO_OFFSET(cpu_info)(tp)

    /* Restore hstatus */
    REG_L   t0, RISCV_CPU_USER_REGS_OFFSET(hstatus)(sp)
    csrw    hstatus, t0

    /* Restore sepc */
    REG_L   t0, RISCV_CPU_USER_REGS_OFFSET(sepc)(sp)
    csrw    sepc, t0

    /* Restore general purpose registers */
    REG_L   a0, RISCV_CPU_USER_REGS_OFFSET(a0)(sp)
    REG_L   a1, RISCV_CPU_USER_REGS_OFFSET(a1)(sp)
    REG_L   a2, RISCV_CPU_USER_REGS_OFFSET(a2)(sp)
    REG_L   a3, RISCV_CPU_USER_REGS_OFFSET(a3)(sp)
    REG_L   a4, RISCV_CPU_USER_REGS_OFFSET(a4)(sp)
    REG_L   a5, RISCV_CPU_USER_REGS_OFFSET(a5)(sp)
    REG_L   a6, RISCV_CPU_USER_REGS_OFFSET(a6)(sp)
    REG_L   a7, RISCV_CPU_USER_REGS_OFFSET(a7)(sp)

    /* Restore return address register */
    REG_L   ra, RISCV_CPU_USER_REGS_OFFSET(ra)(sp)

    REG_L   sp, RISCV_CPU_USER_REGS_OFFSET(sp)(sp)

    /* Back up thread pointer to sscratch */
    csrrw tp, CSR_SCRATCH, tp

    sret

_hang:
    wfi
    j _hang

/* t0 is used as a temporary reg and is clobbered to oblivion */
ENTRY(return_to_new_vcpu64)
    /* Store stack pointer to tp->vcpu->arch.guest_cpu_user_regs.sp */
    REG_S   sp, RISCV_PCPUINFO_OFFSET(cpu_info)(tp)

    /* Backup tp into sscratch */
    csrrw    tp, CSR_SCRATCH, tp

    /* Set vCPU registers */
    REG_L   t0, RISCV_CPU_USER_REGS_OFFSET(sepc)(sp)
    csrw    sepc, t0

    /* Hartid goes to 10 */
    REG_L   a0, RISCV_CPU_USER_REGS_OFFSET(a0)(sp)

    /* DTB goes to a1 */
    REG_L   a1, RISCV_CPU_USER_REGS_OFFSET(a1)(sp)

    /* Set hstatus */
    add     t0, zero, zero
    or      t0, t0, HSTATUS_SPV
    or      t0, t0, HSTATUS_SPVP

    csrw    CSR_HSTATUS, t0

    /* Set guest mode to supervisor */
    li      t0, SSTATUS_SPP
    csrs    CSR_SSTATUS, t0

    /* Enter guest */
    sret

/*
 * struct vcpu *__context_switch(struct vcpu *prev, struct vcpu *next)
 *
 * a0 - prev
 * a1 - next
 *
 * Returns prev in a0
 */
ENTRY(__context_switch)
    /* TODO: support real context switching */
	REG_S	ra, VCPU_SAVED_CONTEXT_OFFSET(pc)(a0)
	REG_S	sp, VCPU_SAVED_CONTEXT_OFFSET(sp)(a0)
	REG_L	sp, VCPU_SAVED_CONTEXT_OFFSET(sp)(a1)
	REG_L	ra, VCPU_SAVED_CONTEXT_OFFSET(pc)(a1)
    ret
ENDPROC(__context_switch)

ENTRY(__riscv_unpriv_trap)
	/*
	 * We assume that faulting unpriv load/store instruction is
	 * 4-byte long and blindly increment SEPC by 4.
	 *
	 * The trap details will be saved at address pointed by 'A0'
	 * register and we use 'A1' register as temporary.
	 */
	csrr	a1, CSR_SEPC
	REG_S	a1, RISCV_TRAP_OFFSET(sepc)(a0)
	addi	a1, a1, 4
	csrw	CSR_SEPC, a1
	csrr	a1, CSR_SCAUSE
	REG_S	a1, RISCV_TRAP_OFFSET(scause)(a0)
	csrr	a1, CSR_STVAL
	REG_S	a1, RISCV_TRAP_OFFSET(stval)(a0)
	csrr	a1, CSR_HTVAL
	REG_S	a1, RISCV_TRAP_OFFSET(htval)(a0)
	csrr	a1, CSR_HTINST
	REG_S	a1, RISCV_TRAP_OFFSET(htinst)(a0)
	sret
END(__riscv_unpriv_trap)
