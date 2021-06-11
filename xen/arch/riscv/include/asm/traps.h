#ifndef __ASM_TRAPS_H__
#define __ASM_TRAPS_H__

#ifndef __ASSEMBLY__

void __handle_exception(void);
void handle_exception(void);

struct riscv_trap {
    unsigned long sepc;
    unsigned long scause;
    unsigned long stval;
    unsigned long htval;
    unsigned long htinst;
};

#endif /* __ASSEMBLY__ */

#define RISCV_TRAP_sepc     0
#define RISCV_TRAP_scause   1
#define RISCV_TRAP_stval    2
#define RISCV_TRAP_htval    3
#define RISCV_TRAP_htinst   4
#define RISCV_TRAP_last     5
#define RISCV_TRAP_OFFSET(x)	((RISCV_TRAP_##x) * __SIZEOF_POINTER__)
#define RISCV_TRAP_SIZE		    RISCV_TRAP_OFFSET(last)


#endif /* __ASM_TRAPS_H__ */


