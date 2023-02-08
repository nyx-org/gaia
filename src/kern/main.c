/* SPDX-License-Identifier: BSD-2-Clause */
#include <libkern/debug.h>
#include <kern/charon.h>
#include <kern/vm/vm.h>
#include <kern/vm/vm_kernel.h>
#include <dev/acpi.h>

void gaia_main(charon_t charon)
{
    vm_init(charon);

    if (charon.rsdp != 0)
        acpi_init(charon);

    machine_init();

    log("gaia (0.0.1)");
    log("Memory stats (pages): ");
    log("vm_kernel: %ld, phys: %ld, total: %ld",
        vm_kernel_stat().in_use / PAGE_SIZE, phys_used_pages(),
        phys_total_pages());
}
