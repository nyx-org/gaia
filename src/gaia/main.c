/*
 * Copyright (c) 2022, lg.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <gaia/base.h>
#include <gaia/firmware/acpi.h>
#include <gaia/firmware/lapic.h>
#include <gaia/host.h>
#include <gaia/pmm.h>
#include <gaia/rights.h>
#include <gaia/sched.h>
#include <gaia/slab.h>
#include <gaia/syscall.h>
#include <gaia/term.h>
#include <gaia/vec.h>
#include <gaia/vm/kmem.h>
#include <gaia/vm/vm_kernel.h>
#include <gaia/vm/vmem.h>
#include <stdbool.h>
#include <stdc-shim/string.h>

#define BOOTSTRAP_SERVER_NAME "/bootstrap"

static Charon *_charon = NULL;

Charon *gaia_get_charon(void)
{
    return _charon;
}

void gaia_main(Charon *charon)
{
    term_init(charon);

    pmm_init(charon);

#ifdef DEBUG
    pmm_dump();
#endif

    host_initialize();

    vmem_bootstrap();
    vm_kernel_init();
    kmem_bootstrap();

    _charon = (void *)host_phys_to_virt((uintptr_t)pmm_alloc_zero());

    host_accelerated_copy(_charon, charon, sizeof(Charon));

    _charon->framebuffer.address = host_virt_to_phys(_charon->framebuffer.address);

    acpi_init(charon->rsdp);
#ifdef DEBUG
    acpi_dump_tables();
#endif
    sched_init();

    syscall_init(*charon);

    bool found = false;
    for (int i = 0; i < charon->modules.count; i++)
    {
        if (strncmp(charon->modules.modules[i].name, BOOTSTRAP_SERVER_NAME, strlen(BOOTSTRAP_SERVER_NAME)) == 0)
        {
            sched_create_new_task_from_elf((uint8_t *)(host_phys_to_virt(charon->modules.modules[i].address)), RIGHT_DMA | RIGHT_REGISTER_DMA);
            found = true;
            break;
        }
    }

    if (!found)
    {
        panic("Cannot find bootstrap server");
    }

    log("-- Memory stats (kb) --");
    log("vm_kernel: %d\t phys: %d", vm_kernel_stat().in_use / 1024, pmm_get_allocated_pages() * PAGE_SIZE / 1024);
    log("gaia (0.0.1-proof-of-concept) finished booting on %s", host_get_name());
    log("Welcome to the machine!");

    host_enable_interrupts();
}
