/*
 * Dummy smpboot support
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
#include <xen/cpu.h>
#include <xen/cpumask.h>
#include <xen/errno.h>
#include <xen/init.h>
#include <xen/sched.h>
#include <xen/smp.h>
#include <xen/nodemask.h>

cpumask_t cpu_online_map;
cpumask_t cpu_present_map;
cpumask_t cpu_possible_map;

DEFINE_PER_CPU(unsigned int, cpu_id);
DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_mask);
DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_core_mask);

/* Fake one node for now. See also include/asm-arm/numa.h */
nodemask_t __read_mostly node_online_map = { { [0] = 1UL } };

/* Boot cpu data */
struct init_info init_data =
{
};

int __cpu_up(unsigned int cpu)
{
    /* TODO */
    BUG();
    return 0;
}

/* Shut down the current CPU */
void __cpu_disable(void)
{
    /* TODO */
    BUG();
}

void __cpu_die(unsigned int cpu)
{
    /* TODO */
    BUG();
}

int __init
smp_get_max_cpus(void)
{
    int i, max_cpus = 0;

    for ( i = 0; i < nr_cpu_ids; i++ )
        if ( cpu_possible(i) )
            max_cpus++;

    return max_cpus;
}

void __init
smp_clear_cpu_maps (void)
{
    cpumask_clear(&cpu_possible_map);
    cpumask_clear(&cpu_online_map);
    cpumask_set_cpu(0, &cpu_possible_map);
    cpumask_set_cpu(0, &cpu_online_map);
    cpumask_copy(&cpu_present_map, &cpu_possible_map);
}
