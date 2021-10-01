#ifndef __ARM_SETUP_H_
#define __ARM_SETUP_H_

#include <xen/bootfdt.h>
#include <public/version.h>
#include <asm/p2m.h>
#include <xen/device_tree.h>

extern domid_t max_init_domid;

void copy_from_paddr(void *dst, paddr_t paddr, unsigned long len);

size_t estimate_efi_size(unsigned int mem_nr_banks);

void acpi_create_efi_system_table(struct domain *d,
                                  struct membank tbl_add[]);

void acpi_create_efi_mmap_table(struct domain *d,
                                const struct meminfo *mem,
                                struct membank tbl_add[]);

int acpi_make_efi_nodes(void *fdt, struct membank tbl_add[]);

void create_domUs(void);
void create_dom0(void);
void alloc_static_evtchn(void);

void discard_initial_modules(void);
void fw_unreserved_regions(paddr_t s, paddr_t e,
                           void (*cb)(paddr_t, paddr_t), unsigned int first);

extern uint32_t hyp_traps_vector[];
void init_traps(void);

#endif
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
