#ifndef __RISCV_PAGE_SHIFT_H__
#define __RISCV_PAGE_SHIFT_H__

#define PAGE_SHIFT              12

#ifdef CONFIG_RISCV_64
#define PADDR_BITS              39
#else
#define PADDR_BITS              32
#endif

#define VADDR_BITS              32

#endif /* __RISCV_PAGE_SHIFT_H__ */
