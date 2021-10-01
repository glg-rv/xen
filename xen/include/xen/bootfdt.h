#ifndef __BOOTFDT_H__
#define __BOOTFDT_H__

#include <xen/types.h>
#include <xen/device_tree.h>
#include <asm/types.h>
#include <public/xen.h>

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
    /* The reserved regions are only used when booting using Device-Tree */
    struct meminfo reserved_mem;
    struct bootmodules modules;
    struct bootcmdlines cmdlines;
#ifdef CONFIG_ACPI
    struct meminfo acpi;
#endif
};

extern struct bootinfo bootinfo;

void device_tree_get_reg(const __be32 **cell, u32 address_cells,
                         u32 size_cells, u64 *start, u64 *size);
u32 device_tree_get_u32(const void *fdt, int node,
                        const char *prop_name, u32 dflt);
size_t boot_fdt_info(const void *fdt, paddr_t paddr);
const char *boot_fdt_cmdline(const void *fdt);
struct bootmodule *add_boot_module(bootmodule_kind kind,
                                          paddr_t start, paddr_t size,
                                          bool domU);
struct bootmodule *boot_module_find_by_kind(bootmodule_kind kind);
void add_boot_cmdline(const char *name, const char *cmdline,
                      bootmodule_kind kind, paddr_t start, bool domU);
struct bootcmdline *boot_cmdline_find_by_kind(bootmodule_kind kind);
struct bootcmdline *boot_cmdline_find_by_name(const char *name);
struct bootmodule *boot_module_find_by_addr_and_kind(bootmodule_kind kind,
                                                             paddr_t start);
const char *boot_module_kind_as_string(bootmodule_kind kind);

#endif /* __BOOTFDT_H__ */
