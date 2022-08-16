/*
 * Copyright (c) 2022, lg
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/** @file
 *  @brief x86_64 Paging implementation.
 * Note that this is only the architecture-related stuff. The actual virtual memory manager is in the root of the gaia source tree.
 */

#ifndef ARCH_X86_64_PAGING_H
#define ARCH_X86_64_PAGING_H
#include <gaia/base.h>

typedef struct
{
    Spinlock lock;
    uint64_t *pml4;
} Pagemap;

#define PTE_PRESENT (1 << 0)
#define PTE_WRITABLE (1 << 1)
#define PTE_USER (1 << 2)
#define PTE_NOT_EXECUTABLE (1 << 63)

#define PTE_ADDR_MASK ~(0xfff)
#define PTE_GET_ADDR(VALUE) ((VALUE)&PTE_ADDR_MASK)
#define PTE_GET_FLAGS(VALUE) ((VALUE) & ~PTE_ADDR_MASK)

#define PTE_IS_PRESENT(x) ((x)&PTE_PRESENT)
#define PTE_IS_WRITABLE(x) ((x)&PTE_WRITABLE)
#define PTE_IS_USER(x) ((x)&PTE_USER)
#define PTE_IS_NOT_EXECUTABLE(x) ((x)&PTE_NOT_EXECUTABLE)

/**
 * @brief Initializes paging.
 *
 */
void paging_initialize(void);

/**
 * @brief Loads a pagemap.
 *
 * @param pagemap The pagemap to load.
 */
void paging_load_pagemap(Pagemap *pagemap);

/**
 * @brief Maps a physical page to a virtual address.
 *
 * @param pagemap The pagemap in which the mapping should be made.
 * @param vaddr The virtual address to which the page should be mapped.
 * @param paddr The physical address of the page to map.
 * @param flags The flags to set for the mapping (See the PTE_ macros).
 */
void paging_map_page(Pagemap *pagemap, uintptr_t vaddr, uintptr_t paddr, uint64_t flags);

/**
 * @brief Unmaps a virtual address.
 *
 * @param pagemap The pagemap in which the mapping should be made.
 * @param vaddr The virtual address to which the page should be mapped.
 */
void paging_unmap_page(Pagemap *pagemap, uintptr_t vaddr);

/** @brief Gets the main kernel pagemap. */
Pagemap *paging_get_kernel_pagemap(void);

#endif /* ARCH_X86_64_PAGING_H */