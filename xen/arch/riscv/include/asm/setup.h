#ifndef __RISCV_SETUP_H_
#define __RISCV_SETUP_H_

#include <public/version.h>

#define MIN_FDT_ALIGN 8
#define MAX_FDT_SIZE SZ_2M

#define NR_MEM_BANKS 128

#define MAX_MODULES 32 /* Current maximum useful modules */

typedef enum {
    BOOTMOD_XEN,
    BOOTMOD_FDT,
    BOOTMOD_KERNEL,
    BOOTMOD_RAMDISK,
    BOOTMOD_XSM,
    BOOTMOD_GUEST_DTB,
    BOOTMOD_UNKNOWN
}  bootmodule_kind;


struct membank {
    paddr_t start;
    paddr_t size;
};

struct meminfo {
    int nr_banks;
    struct membank bank[NR_MEM_BANKS];
};

/*
 * The domU flag is set for kernels and ramdisks of "xen,domain" nodes.
 * The purpose of the domU flag is to avoid getting confused in
 * kernel_probe, where we try to guess which is the dom0 kernel and
 * initrd to be compatible with all versions of the multiboot spec. 
 */
#define BOOTMOD_MAX_CMDLINE 1024
struct bootmodule {
    bootmodule_kind kind;
    bool domU;
    paddr_t start;
    paddr_t size;
};

/* DT_MAX_NAME is the node name max length according the DT spec */
#define DT_MAX_NAME 41
struct bootcmdline {
    bootmodule_kind kind;
    bool domU;
    paddr_t start;
    char dt_name[DT_MAX_NAME];
    char cmdline[BOOTMOD_MAX_CMDLINE];
};

struct bootmodules {
    int nr_mods;
    struct bootmodule module[MAX_MODULES];
};

struct bootcmdlines {
    unsigned int nr_mods;
    struct bootcmdline cmdline[MAX_MODULES];
};

struct bootinfo {
    struct meminfo mem;
    struct meminfo reserved_mem;
    struct bootmodules modules;
    struct bootcmdlines cmdlines;
#ifdef CONFIG_ACPI
    struct meminfo acpi;
#endif
};

extern struct bootinfo bootinfo;

//extern domid_t max_init_domid;

#define max_init_domid (0)

struct bootmodule *boot_module_find_by_kind(bootmodule_kind kind);
struct bootcmdline * __init boot_cmdline_find_by_kind(bootmodule_kind kind);
struct bootcmdline * __init boot_cmdline_find_by_name(const char *name);
struct bootmodule * __init boot_module_find_by_addr_and_kind(bootmodule_kind kind,
                                                             paddr_t start);

#define NR_VCPUS 2
#define DOM0_KERNEL 0x80400000
#define DOM0_KERNEL_SIZE 5704861
#define DOM0_INITRD 0x0
#define DOM0_INITRD_SIZE 0x0
#define DTB 0x80972000
#define DTB_SIZE 3686

/* Devices */
#define PLIC_BASE  0xc000000
#define PLIC_SIZE  0x0210000
#define PLIC_END (PLIC_BASE + PLIC_SIZE)

#endif /* __RISCV_SETUP_H_ */
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
