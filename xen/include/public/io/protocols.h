/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * protocols.h
 *
 * Copyright (c) 2008, Keir Fraser
 */

#ifndef __XEN_PROTOCOLS_H__
#define __XEN_PROTOCOLS_H__

#define XEN_IO_PROTO_ABI_X86_32     "x86_32-abi"
#define XEN_IO_PROTO_ABI_X86_64     "x86_64-abi"
#define XEN_IO_PROTO_ABI_ARM        "arm-abi"
#define XEN_IO_PROTO_ABI_RISCV      "riscv-abi"

#if defined(__i386__)
# define XEN_IO_PROTO_ABI_NATIVE XEN_IO_PROTO_ABI_X86_32
#elif defined(__x86_64__)
# define XEN_IO_PROTO_ABI_NATIVE XEN_IO_PROTO_ABI_X86_64
#elif defined(__arm__) || defined(__aarch64__)
# define XEN_IO_PROTO_ABI_NATIVE XEN_IO_PROTO_ABI_ARM
#elif defined(__riscv)
# define XEN_IO_PROTO_ABI_NATIVE XEN_IO_PROTO_ABI_RISCV
#else
# error arch fixup needed here
#endif

#endif
