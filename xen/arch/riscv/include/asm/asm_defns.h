#ifndef __RISCV_ASM_DEFNS_H__
#define __RISCV_ASM_DEFNS_H__

#ifndef COMPILE_OFFSETS
/* NB. Auto-generated from arch/.../asm-offsets.c */
#include <asm/asm-offsets.h>
#endif
#include <asm/processor.h>

#define INTEGER_ALIGN (__riscv_xlen / 8)

#define ASM_INT(label, val)                 \
    .align INTEGER_ALIGN;                   \
label:                                      \
    .word val;

#endif /* __RISCV_ASM_DEFNS_H__ */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
