/* SPDX-License-Identifier: BSD-2-Clause */
#include <machdep/cpu.h>
#include <x86_64/asm.h>

void cpu_halt(void)
{
    while (true) {
        hlt();
    }
}

void cpu_enable_interrupts(void)
{
    sti();
}