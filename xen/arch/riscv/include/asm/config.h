#ifndef __RISCV_CONFIG_H__
#define __RISCV_CONFIG_H__

#include <xen/const.h>
#include <xen/page-size.h>
#include <asm/page-bits.h>

/*
 * RISC-V Layout:
 *   0  -   2M   Unmapped
 *   2M -   4M   Xen text, data, bss
 *   4M -   6M   Fixmap: special-purpose 4K mapping slots
 *   6M -  10M   Early boot mapping of FDT
 *   10M - 12M   Early relocation address (used when relocating Xen)
 *               and later for livepatch vmap (if compiled in)
 *
 *   All of the above is mapped in L2 slot[0] (except for Unmapped)
 *
 *   1G - 2G   VMAP: ioremap and early_ioremap (L2 slot 2)
 *
 *   2G - 5G: Unused
 *
 *   5G - 8G
 *   0x140000000 - 0x200000000
 *   Frametable: 24 bytes per page for 371GB of RAM, GB-aligned (slightly over 2GB, L2 slots [6..7])
 *
 *   8G - 12G : Unused
 *
 *   0x300000000  - 0x5fffffffff : 371GB, L2 Slots [12...384)
 *   1:1 mapping of RAM
 *
 *   0x6000000000 - 0x7fffffffff : 127GB, L2 slots [384..512)
 *   Unused
 */

#if defined(CONFIG_RISCV_64)
# define LONG_BYTEORDER 3
# define ELFSIZE 64
# define MAX_VIRT_CPUS 128u
#else
# error "Unsupported RISCV variant"
#endif

#define BYTES_PER_LONG (1 << LONG_BYTEORDER)
#define BITS_PER_LONG  (BYTES_PER_LONG << 3)
#define POINTER_ALIGN  BYTES_PER_LONG

#define BITS_PER_LLONG 64

/* xen_ulong_t is always 64 bits */
#define BITS_PER_XEN_ULONG 64

#define CONFIG_RISCV_L1_CACHE_SHIFT 6
#define CONFIG_PAGEALLOC_MAX_ORDER  18
#define CONFIG_DOMU_MAX_ORDER       9
#define CONFIG_HWDOM_MAX_ORDER      10

#define OPT_CONSOLE_STR "dtuart"
#define INVALID_VCPU_ID MAX_VIRT_CPUS

#ifdef CONFIG_RISCV_64

/* Bit counts for virtual address fields (sv39) */
#define VPN2_BITS   (9)
#define VPN1_BITS   (9)
#define VPN0_BITS   (9)
#define OFFSET_BITS (12)

/* SLOT2_ENTRY_BITS == 30 */
#define SLOT2_ENTRY_BITS  (VPN1_BITS + VPN2_BITS + OFFSET_BITS)
#define SLOT2(slot) (_AT(vaddr_t,slot) << SLOT2_ENTRY_BITS)
#define SLOT2_ENTRY_SIZE  SLOT2(1)

#define DIRECTMAP_VIRT_START   SLOT2(12)

/* See above "RISC-V Layout" for description of layout (and
 * where these magic numbers come from */
#define DIRECTMAP_SIZE         (SLOT2_ENTRY_SIZE * (384-12))
#define DIRECTMAP_VIRT_END     (DIRECTMAP_VIRT_START + DIRECTMAP_SIZE - 1)
#define XENHEAP_VIRT_START     xenheap_virt_start
#define HYPERVISOR_VIRT_END    DIRECTMAP_VIRT_END

#else /* RISCV_32 */
#error "RISC-V 32-bit is not supported yet"
#endif

#define FIXMAP_CONSOLE  0  /* The primary UART */
#define FIXMAP_MISC     1  /* Ephemeral mappings of hardware */

#define CONFIG_PAGING_LEVELS 3

#define CONFIG_PAGEALLOC_MAX_ORDER 18
#define CONFIG_DOMU_MAX_ORDER      9
#define CONFIG_HWDOM_MAX_ORDER     10

#ifdef CONFIG_RISCV_64
#define MAX_VIRT_CPUS 128u
#else
#define MAX_VIRT_CPUS 8u
#endif

#define XEN_VIRT_START         _AT(vaddr_t,0x00200000)
#define XEN_VIRT_END           _AT(vaddr_t,0x40000000)
#define FIXMAP_ADDR(n)        (_AT(vaddr_t,0x00400000) + (n) * PAGE_SIZE)

#define HYPERVISOR_VIRT_START  XEN_VIRT_START

#define INVALID_VCPU_ID MAX_VIRT_CPUS

#define STACK_ORDER 3
#define STACK_SIZE  (PAGE_SIZE << STACK_ORDER)

#define VMAP_VIRT_START  GB(1)
#define VMAP_VIRT_END    (VMAP_VIRT_START + GB(1))

#define FRAMETABLE_VIRT_START  GB(5)
#define FRAMETABLE_SIZE        GB(1)
#define FRAMETABLE_NR          (FRAMETABLE_SIZE / sizeof(*frame_table))
#define FRAMETABLE_VIRT_END    (FRAMETABLE_VIRT_START + FRAMETABLE_SIZE - 1)

/**
 * All RISC-V implementations (except for with C extension) enforce 32-bit
 * instruction address alignment.  With C extension, 16-bit and 32-bit are
 * both allowed.
 */
#ifndef __ALIGN
#define __ALIGN     .align 4
#define __ALIGN_STR ".align 4"
#endif

/* Linkage for RISCV */
#ifdef __ASSEMBLY__
#define ALIGN __ALIGN
#define ALIGN_STR __ALIGN_STR

#define ENTRY(name)                                     \
  .globl name;                                          \
  ALIGN;                                                \
  name:

#define GLOBAL(name)                                    \
  .globl name;                                          \
  name:

#define WEAK(name)                                      \
  .weak name;                                           \
  ALIGN;                                                \
  name:

#define END(name)                                       \
  .size name, .-name

#ifndef ASM_NL
#define ASM_NL		 ;
#endif

#define SYM_END(name, sym_type)				            \
  .type name sym_type ASM_NL			                \
  .size name, .-name

#define SYM_FUNC_END(name)				                \
  SYM_END(name, STT_FUNC)

/* If symbol 'name' is treated as a subroutine (gets called, and returns)
 * then please use ENDPROC to mark 'name' as STT_FUNC for the benefit of
 * static analysis tools such as stack depth analyzer.
 */
#define ENDPROC(name) \
  SYM_FUNC_END(name)

#define __PAGE_ALIGNED_DATA	.section ".data..page_aligned", "aw"
#define __PAGE_ALIGNED_BSS	.section ".bss..page_aligned", "aw"
#endif

#endif /* __RISCV_CONFIG_H__ */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
