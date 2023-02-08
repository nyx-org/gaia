/* SPDX-License-Identifier: BSD-2-Clause */
/**
 * @file charon.h
 * @brief Charon is gaia's generic (e.g bootloader-independent) boot protocol.
 *
 * A Charon object is generated by the architecture-dependent code then it is passed to gaia_main(), this ensures a unified kernel entry.
 */
#ifndef SRC_KERN_CHARON_H_
#define SRC_KERN_CHARON_H_
#include <libkern/base.h>

#define CHARON_MODULE_MAX 16
#define CHARON_MMAP_SIZE_MAX 128

/**
 * Memory map entry type
 */
enum charon_mmap_entry_type {
    MMAP_FREE, /**< Denotes a free memory map entry */
    MMAP_RESERVED, /**< Denotes a reserved memory map entry */
    MMAP_MODULE, /**< Denotes a memory map entry that stores bootloader modules */
    MMAP_RECLAIMABLE, /**< Denotes a reclaimable memory map entry */
    MMAP_FRAMEBUFFER /**< Denotes a memory map entry that contains the framebuffer */
};

/** Describes a memory map entry */
typedef struct {
    uintptr_t base; /**< Base address */
    size_t size; /**< Size */
    enum charon_mmap_entry_type type; /**< Type */
} charon_mmap_entry_t;

/** Describes the physical memory map */
typedef struct {
    uint8_t count; /**< Number of entries */
    charon_mmap_entry_t entries[CHARON_MMAP_SIZE_MAX]; /**< Actual entries */
} charon_mmap_t;

/** Describes the framebuffer */
typedef struct {
    bool present : 1; /**< Whether or not there's a framebuffer */
    uintptr_t address; /**< The address of the framebuffer */
    uint32_t width, height, pitch, bpp;
} charon_framebuffer_t;

/** Describes a single bootloader module */
typedef struct {
    const char *name; /**< Name of the module */
    size_t size; /**< Size of the module */
    uintptr_t address; /**< Base address of the module */
} charon_module_t;

typedef struct {
    uint8_t count; /**< Number of modules */
    charon_module_t modules[CHARON_MODULE_MAX]; /**< Actual modules */
} charon_modules_t;

typedef struct {
    uintptr_t rsdp; /**< Root system description pointer. Used by ACPI routines. */
    charon_framebuffer_t framebuffer; /**< The framebuffer */
    charon_mmap_t memory_map; /**< The memory map */
    charon_modules_t modules; /**< Bootloader modules */
} charon_t;

#endif
