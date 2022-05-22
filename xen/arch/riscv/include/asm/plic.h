/*
 * RISC-V Platform-Level Interrupt Controller support
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

#ifndef __ASM_RISCV_PLIC_H__
#define __ASM_RISCV_PLIC_H__

/* Find the interrupt controller and set up the callback to translate
 * device tree IRQ.
 */
extern void plic_preinit(void);

#endif
