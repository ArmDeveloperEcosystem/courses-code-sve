// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear

#if defined(__APPLE__)
#define ASM_SYMBOL(name) _##name
#else
#define ASM_SYMBOL(name) name
#endif

    .text
    .globl ASM_SYMBOL(example_add_one_asm)
#if !defined(__APPLE__)
    .type ASM_SYMBOL(example_add_one_asm), %function
#endif
ASM_SYMBOL(example_add_one_asm):
    add w0, w0, #1
    ret
#if !defined(__APPLE__)
    .size ASM_SYMBOL(example_add_one_asm), . - ASM_SYMBOL(example_add_one_asm)
#endif
