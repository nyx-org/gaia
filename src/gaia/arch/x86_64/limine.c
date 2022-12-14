/*
 * Copyright (c) 2022, lg
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "gaia/host.h"
#include <gaia/charon.h>
#include <gaia/limine.h>
#include <stdc-shim/string.h>

volatile static struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0};

volatile static struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0};

volatile static struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0};

volatile static struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0};

static void limine_fb_to_charon(struct limine_framebuffer *fb, CharonFramebuffer *ret)
{
    *ret = (CharonFramebuffer){
        .address = (uintptr_t)fb->address,
        .width = fb->width,
        .height = fb->height,
        .pitch = fb->pitch,
        .bpp = fb->bpp,
        .present = true};
}

static CharonMemoryMapEntryType limine_mmap_type_to_charon(uint64_t type)
{
    switch (type)
    {
    case LIMINE_MEMMAP_USABLE:
        return MMAP_FREE;
    case LIMINE_MEMMAP_RESERVED:
        return MMAP_RESERVED;
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        return MMAP_RECLAIMABLE;
    case LIMINE_MEMMAP_ACPI_NVS:
        return MMAP_RESERVED;
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        return MMAP_RECLAIMABLE;
    case LIMINE_MEMMAP_KERNEL_AND_MODULES:
        return MMAP_MODULE;
    case LIMINE_MEMMAP_FRAMEBUFFER:
        return MMAP_FRAMEBUFFER;
    default:
        return MMAP_RESERVED;
    }
}

static void limine_mmap_to_charon(struct limine_memmap_response *mmap, CharonMemoryMap *ret)
{
    ret->count = mmap->entry_count;

    assert(ret->count <= CHARON_MMAP_SIZE_MAX);
    for (size_t i = 0; i < mmap->entry_count; i++)
    {
        ret->entries[i] = (CharonMemoryMapEntry){
            .type = limine_mmap_type_to_charon(mmap->entries[i]->type),
            .base = mmap->entries[i]->base,
            .size = mmap->entries[i]->length};
    }
}

static void limine_modules_to_charon(struct limine_module_response *modules, CharonModules *ret)
{
    ret->count = modules->module_count;
    for (size_t i = 0; i < ret->count; i++)
    {
        ret->modules[i].address = host_virt_to_phys((uintptr_t)modules->modules[i]->address);
        ret->modules[i].size = modules->modules[i]->size;
        host_accelerated_copy((void *)ret->modules[i].name, modules->modules[i]->path, strlen(modules->modules[i]->path));
    }
}

void limine_to_charon(Charon *ret)
{
    if (rsdp_request.response)
    {
        ret->rsdp = (uintptr_t)rsdp_request.response->address;
    }

    if (fb_request.response && fb_request.response->framebuffer_count != 0)
    {
        limine_fb_to_charon(fb_request.response->framebuffers[0], &ret->framebuffer);
    }

    if (memmap_request.response)
    {
        limine_mmap_to_charon(memmap_request.response, &ret->memory_map);
    }

    if (module_request.response)
    {
        limine_modules_to_charon(module_request.response, &ret->modules);
    }
}
