/*
 * RISC-V Interrupt support
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

#include <xen/lib.h>
#include <xen/errno.h>
#include <xen/sched.h>
#include <xen/device_tree.h>

const unsigned int nr_irqs = NR_IRQS;

/* Describe an IRQ assigned to a guest */
struct irq_guest
{
    struct domain *d;
    unsigned int virq;
};

static void ack_none(struct irq_desc *irq)
{
    /* TODO */
    BUG();
}

static void end_none(struct irq_desc *irq)
{
    /* TODO */
    BUG();
}

hw_irq_controller no_irq_type = {
    .typename = "none",
    .startup = irq_startup_none,
    .shutdown = irq_shutdown_none,
    .enable = irq_enable_none,
    .disable = irq_disable_none,
    .ack = ack_none,
    .end = end_none
};

int arch_init_one_irq_desc(struct irq_desc *desc)
{
    /* TODO */
    BUG();
    return 0;
}

struct pirq *alloc_pirq_struct(struct domain *d)
{
	/* TODO */
    BUG();
    return NULL;
}

irq_desc_t *__irq_to_desc(int irq)
{
    /* TODO */
    BUG();
    return 0;
}

int pirq_guest_bind(struct vcpu *v, struct pirq *pirq, int will_share)
{
    BUG();
}

void pirq_guest_unbind(struct domain *d, struct pirq *pirq)
{

    BUG();
}

void pirq_set_affinity(struct domain *d, int pirq, const cpumask_t *mask)
{
    BUG();
}

void smp_send_state_dump(unsigned int cpu)
{
    /* TODO */
}

void arch_move_irqs(struct vcpu *v)
{
    /* TODO */
}

int setup_irq(unsigned int irq, unsigned int irqflags, struct irqaction *new)
{
    /* TODO */
    BUG();
    return -ENOSYS;;
}

int platform_get_irq(const struct dt_device_node *device, int index)
{
    struct dt_irq dt_irq;
    unsigned int irq;

    if ( dt_device_get_irq(device, index, &dt_irq) )
        return -1;

    irq = dt_irq.irq;

    return irq;
}

int irq_set_spi_type(unsigned int spi, unsigned int type)
{
    return 0;
}

int irq_set_type(unsigned int irq, unsigned int type)
{
    return 0;
}
