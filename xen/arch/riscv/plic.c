/*
 * xen/arch/riscv/plic.c
 *
 * RISC-V Platform-Level Interrupt Controller support
 *
 * Based on xen/arch/arm/gic.c, xen/arch/arm/gic-v2.c
 * Tim Deegan <tim@xen.org>
 * Copyright (c) 2011 Citrix Systems.
 *
 * Copyright (c) 2022 Xie Xun <xiexun162534@gmail.com> 
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

#include <xen/errno.h>
#include <asm/device.h>
#include <asm/plic.h>
#include <asm/vplic.h>
#include <asm/acpi.h>


static void __init plic_dt_preinit(void)
{
    int rc;
    struct dt_device_node *node;
    uint8_t num_plics = 0;

    dt_for_each_device_node( dt_host, node )
    {
        if ( !dt_get_property(node, "interrupt-controller", NULL) )
            continue;

        if ( !dt_get_parent(node) )
            continue;

        rc = device_init(node, DEVICE_GIC, NULL);
        if ( !rc )
        {
            /* NOTE: Only one PLIC is supported */
            num_plics = 1;
            break;
        }
    }
    if ( !num_plics )
        panic("Unable to find compatible PLIC in the device tree\n");

    /* Set the PLIC as the primary interrupt controller */
    dt_interrupt_controller = node;
    dt_device_set_used_by(node, DOMID_XEN);
}

/* Find the interrupt controller and set up the callback to translate
 * device tree or ACPI IRQ.
 */
void __init plic_preinit(void)
{
    if ( acpi_disabled )
        plic_dt_preinit();
    else
        panic("TODO APIC\n");
}

int plic_irq_xlate(const u32 *intspec, unsigned int intsize,
                   unsigned int *out_hwirq,
                   unsigned int *out_type)
{
    /* TODO */
    return -EINVAL;
}

static const struct dt_device_match plic_dt_match[] __initconst =
{
    DT_MATCH_COMPATIBLE("riscv,plic0"),
    { /* sentinel */ },
};

static int __init plic_dev_dt_preinit(struct dt_device_node *node,
                                      const void *data)
{
    /* TODO */
    dt_irq_xlate = plic_irq_xlate;

    return 0;
}

DT_DEVICE_START(plic, "PLIC", DEVICE_GIC)
        .dt_match = plic_dt_match,
        .init = plic_dev_dt_preinit,
DT_DEVICE_END
