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
    csrrw   tp, CSR_SSCRATCH, tp

    /* Based on Linux kenrel arch/riscv/entry.S */
    bnez    tp, save_to_cpuinfo

    /* Exceptions from xen */
save_to_stack:
    /* Save context to stack */
    REG_S   sp, (RISCV_CPU_USER_REGS_OFFSET(sp) - RISCV_CPU_USER_REGS_SIZE) (sp)
    addi    sp, sp, -RISCV_CPU_USER_REGS_SIZE
    REG_S   t0, RISCV_CPU_USER_REGS_OFFSET(t0)(sp)
    j       save_context

    /* Exceptions from guest */
save_to_cpuinfo:
    /* tp->tmp = sp */
    REG_S   sp, RISCV_PCPUINFO_OFFSET(tmp)(tp)

    /* sp = ((struct pcpu_info*)tp)->guest_cpu_info */
    REG_L   sp, RISCV_PCPUINFO_OFFSET(guest_cpu_info)(tp)

    /* sp->guest_cpu_info->sp = tp->tmp */
    REG_S   t0, RISCV_CPU_USER_REGS_OFFSET(t0)(sp)
    REG_L   t0, RISCV_PCPUINFO_OFFSET(tmp)(tp)
    REG_S   t0, RISCV_CPU_USER_REGS_OFFSET(sp)(sp)

save_context:
    /* Save registers */
    REG_S   ra, RISCV_CPU_USER_REGS_OFFSET(ra)(sp)
    REG_S   gp, RISCV_CPU_USER_REGS_OFFSET(gp)(sp)
    REG_S   t1, RISCV_CPU_USER_REGS_OFFSET(t1)(sp)
    REG_S   t2, RISCV_CPU_USER_REGS_OFFSET(t2)(sp)
    REG_S   s0, RISCV_CPU_USER_REGS_OFFSET(s0)(sp)
    REG_S   s1, RISCV_CPU_USER_REGS_OFFSET(s1)(sp)
    REG_S   a0, RISCV_CPU_USER_REGS_OFFSET(a0)(sp)
    REG_S   a1, RISCV_CPU_USER_REGS_OFFSET(a1)(sp)
    REG_S   a2, RISCV_CPU_USER_REGS_OFFSET(a2)(sp)
    REG_S   a3, RISCV_CPU_USER_REGS_OFFSET(a3)(sp)
    REG_S   a4, RISCV_CPU_USER_REGS_OFFSET(a4)(sp)
    REG_S   a5, RISCV_CPU_USER_REGS_OFFSET(a5)(sp)
    REG_S   a6, RISCV_CPU_USER_REGS_OFFSET(a6)(sp)
    REG_S   a7, RISCV_CPU_USER_REGS_OFFSET(a7)(sp)
    REG_S   s2, RISCV_CPU_USER_REGS_OFFSET(s2)(sp)
    REG_S   s3, RISCV_CPU_USER_REGS_OFFSET(s3)(sp)
    REG_S   s4, RISCV_CPU_USER_REGS_OFFSET(s4)(sp)
    REG_S   s5, RISCV_CPU_USER_REGS_OFFSET(s5)(sp)
    REG_S   s6, RISCV_CPU_USER_REGS_OFFSET(s6)(sp)
    REG_S   s7, RISCV_CPU_USER_REGS_OFFSET(s7)(sp)
    REG_S   s8, RISCV_CPU_USER_REGS_OFFSET(s8)(sp)
    REG_S   s9, RISCV_CPU_USER_REGS_OFFSET(s9)(sp)
    REG_S   s10, RISCV_CPU_USER_REGS_OFFSET(s10)(sp)
    REG_S   s11, RISCV_CPU_USER_REGS_OFFSET(s11)(sp)
    REG_S   t3, RISCV_CPU_USER_REGS_OFFSET(t3)(sp)
    REG_S   t4, RISCV_CPU_USER_REGS_OFFSET(t4)(sp)
    REG_S   t5, RISCV_CPU_USER_REGS_OFFSET(t5)(sp)
    REG_S   t6, RISCV_CPU_USER_REGS_OFFSET(t6)(sp)
    csrr    t0, CSR_SEPC
    REG_S   t0, RISCV_CPU_USER_REGS_OFFSET(sepc)(sp)
    csrr    t0, CSR_SSTATUS
    REG_S   t0, RISCV_CPU_USER_REGS_OFFSET(sstatus)(sp)

    /* Save guest/xen tp. Set sscratch to zero */
    csrrw   tp, CSR_SSCRATCH, tp
    REG_S   tp, RISCV_CPU_USER_REGS_OFFSET(tp)(sp)
    mv      t0, tp
    csrrw   tp, CSR_SSCRATCH, zero
    bnez    tp, continue_save_context
    /* if tp == 0 */
    mv      tp, t0

continue_save_context:
    /* new_stack_cpu_regs.pregs = old_stack_cpu_res */
    REG_L   t0, RISCV_PCPUINFO_OFFSET(stack_cpu_regs)(tp)
    REG_S   t0, RISCV_CPU_USER_REGS_OFFSET(pregs)(sp)
    /* Update stack_cpu_regs */
    REG_S   sp, RISCV_PCPUINFO_OFFSET(stack_cpu_regs)(tp)
    
    jal     __handle_exception

    /* if trapped from guest, save tp */
    jal     __trap_from_guest
    beqz    a0, restore_registers
    csrw    CSR_SSCRATCH, tp

restore_registers:
    /* Restore stack_cpu_regs */
    REG_L   t0, RISCV_CPU_USER_REGS_OFFSET(pregs)(sp)
    REG_S   t0, RISCV_PCPUINFO_OFFSET(stack_cpu_regs)(tp)

    REG_L   t0, RISCV_CPU_USER_REGS_OFFSET(sepc)(sp)
    csrw    CSR_SEPC, t0
    REG_L   t0, RISCV_CPU_USER_REGS_OFFSET(sstatus)(sp)
    csrw    CSR_SSTATUS, t0

    REG_L   ra, RISCV_CPU_USER_REGS_OFFSET(ra)(sp)
    REG_L   gp, RISCV_CPU_USER_REGS_OFFSET(gp)(sp)
    REG_L   t0, RISCV_CPU_USER_REGS_OFFSET(t0)(sp)
    REG_L   t1, RISCV_CPU_USER_REGS_OFFSET(t1)(sp)
    REG_L   t2, RISCV_CPU_USER_REGS_OFFSET(t2)(sp)
    REG_L   s0, RISCV_CPU_USER_REGS_OFFSET(s0)(sp)
    REG_L   s1, RISCV_CPU_USER_REGS_OFFSET(s1)(sp)
    REG_L   a0, RISCV_CPU_USER_REGS_OFFSET(a0)(sp)
    REG_L   a1, RISCV_CPU_USER_REGS_OFFSET(a1)(sp)
    REG_L   a2, RISCV_CPU_USER_REGS_OFFSET(a2)(sp)
    REG_L   a3, RISCV_CPU_USER_REGS_OFFSET(a3)(sp)
    REG_L   a4, RISCV_CPU_USER_REGS_OFFSET(a4)(sp)
    REG_L   a5, RISCV_CPU_USER_REGS_OFFSET(a5)(sp)
    REG_L   a6, RISCV_CPU_USER_REGS_OFFSET(a6)(sp)
    REG_L   a7, RISCV_CPU_USER_REGS_OFFSET(a7)(sp)
    REG_L   s2, RISCV_CPU_USER_REGS_OFFSET(s2)(sp)
    REG_L   s3, RISCV_CPU_USER_REGS_OFFSET(s3)(sp)
    REG_L   s4, RISCV_CPU_USER_REGS_OFFSET(s4)(sp)
    REG_L   s5, RISCV_CPU_USER_REGS_OFFSET(s5)(sp)
    REG_L   s6, RISCV_CPU_USER_REGS_OFFSET(s6)(sp)
    REG_L   s7, RISCV_CPU_USER_REGS_OFFSET(s7)(sp)
    REG_L   s8, RISCV_CPU_USER_REGS_OFFSET(s8)(sp)
    REG_L   s9, RISCV_CPU_USER_REGS_OFFSET(s9)(sp)
    REG_L   s10, RISCV_CPU_USER_REGS_OFFSET(s10)(sp)
    REG_L   s11, RISCV_CPU_USER_REGS_OFFSET(s11)(sp)
    REG_L   t3, RISCV_CPU_USER_REGS_OFFSET(t3)(sp)
    REG_L   t4, RISCV_CPU_USER_REGS_OFFSET(t4)(sp)
    REG_L   t5, RISCV_CPU_USER_REGS_OFFSET(t5)(sp)
    REG_L   t6, RISCV_CPU_USER_REGS_OFFSET(t6)(sp)

    /* Restore tp */
    REG_L   tp, RISCV_CPU_USER_REGS_OFFSET(tp)(sp)
    /* Restore sp */
    REG_L   sp, RISCV_CPU_USER_REGS_OFFSET(sp)(sp)

    sret

_hang:
    wfi
    j _hang

/* t0 is used as a temporary reg and is clobbered to oblivion */
ENTRY(return_to_new_vcpu64)
    jal     leave_hypervisor_to_guest
    
    /* Store stack pointer to tp->vcpu->arch.guest_cpu_user_regs.sp */
    REG_S   sp, RISCV_PCPUINFO_OFFSET(guest_cpu_info)(tp)

    /* Backup tp into sscratch */
    csrrw    tp, CSR_SSCRATCH, tp

    /* Set vCPU registers */
    REG_L   t0, RISCV_CPU_USER_REGS_OFFSET(sepc)(sp)
    csrw    sepc, t0

    /* Hartid goes to a0 */
    REG_L   a0, RISCV_CPU_USER_REGS_OFFSET(a0)(sp)

    /* DTB goes to a1 */
    REG_L   a1, RISCV_CPU_USER_REGS_OFFSET(a1)(sp)

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
	REG_S	s0, VCPU_SAVED_CONTEXT_OFFSET(s0)(a0)
	REG_S	s1, VCPU_SAVED_CONTEXT_OFFSET(s1)(a0)
	REG_S	s2, VCPU_SAVED_CONTEXT_OFFSET(s2)(a0)
	REG_S	s3, VCPU_SAVED_CONTEXT_OFFSET(s3)(a0)
	REG_S	s4, VCPU_SAVED_CONTEXT_OFFSET(s4)(a0)
	REG_S	s5, VCPU_SAVED_CONTEXT_OFFSET(s5)(a0)
	REG_S	s6, VCPU_SAVED_CONTEXT_OFFSET(s6)(a0)
	REG_S	s7, VCPU_SAVED_CONTEXT_OFFSET(s7)(a0)
	REG_S	s8, VCPU_SAVED_CONTEXT_OFFSET(s8)(a0)
	REG_S	s9, VCPU_SAVED_CONTEXT_OFFSET(s9)(a0)
	REG_S	s10, VCPU_SAVED_CONTEXT_OFFSET(s10)(a0)
	REG_S	s11, VCPU_SAVED_CONTEXT_OFFSET(s11)(a0)
	REG_S	sp, VCPU_SAVED_CONTEXT_OFFSET(sp)(a0)
	REG_S	gp, VCPU_SAVED_CONTEXT_OFFSET(gp)(a0)
	REG_S	ra, VCPU_SAVED_CONTEXT_OFFSET(ra)(a0)
    
	REG_L	s0, VCPU_SAVED_CONTEXT_OFFSET(s0)(a1)
	REG_L	s1, VCPU_SAVED_CONTEXT_OFFSET(s1)(a1)
	REG_L	s2, VCPU_SAVED_CONTEXT_OFFSET(s2)(a1)
	REG_L	s3, VCPU_SAVED_CONTEXT_OFFSET(s3)(a1)
	REG_L	s4, VCPU_SAVED_CONTEXT_OFFSET(s4)(a1)
	REG_L	s5, VCPU_SAVED_CONTEXT_OFFSET(s5)(a1)
	REG_L	s6, VCPU_SAVED_CONTEXT_OFFSET(s6)(a1)
	REG_L	s7, VCPU_SAVED_CONTEXT_OFFSET(s7)(a1)
	REG_L	s8, VCPU_SAVED_CONTEXT_OFFSET(s8)(a1)
	REG_L	s9, VCPU_SAVED_CONTEXT_OFFSET(s9)(a1)
	REG_L	s10, VCPU_SAVED_CONTEXT_OFFSET(s10)(a1)
	REG_L	s11, VCPU_SAVED_CONTEXT_OFFSET(s11)(a1)
	REG_L	sp, VCPU_SAVED_CONTEXT_OFFSET(sp)(a1)
	REG_L	gp, VCPU_SAVED_CONTEXT_OFFSET(gp)(a1)
	REG_L	ra, VCPU_SAVED_CONTEXT_OFFSET(ra)(a1)

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
