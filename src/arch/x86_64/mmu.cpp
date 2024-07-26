#include "lib/error.hpp"
#include <hal/hal.hpp>
#include <hal/mmu.hpp>
#include <vm/phys.hpp>
#include <vm/vm.hpp>
#include <x86_64/asm.hpp>
#include <x86_64/cpuid.hpp>
#include <x86_64/limine.h>
#include <x86_64/mmu.hpp>

using namespace Gaia::x86_64;
using namespace Gaia;

namespace Gaia::Hal::Vm {

static bool cpu_supports_1gb_pages = false;

extern "C" {
extern const char text_start_addr[], text_end_addr[];
extern const char rodata_start_addr[], rodata_end_addr[];
extern const char data_start_addr[], data_end_addr[];
}

static volatile struct limine_kernel_address_request kaddr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0, .response = nullptr};

static uint64_t *get_next_level(uint64_t *table, size_t index, bool allocate) {

  // If the entry is already present, just return it.
  if (PTE_IS_PRESENT(table[index])) {
    return reinterpret_cast<uint64_t *>(
        Hal::phys_to_virt(PTE_GET_ADDR(table[index])));
  }

  // If there's no entry in the page table, and we don't want to allocate, we
  // abort.
  if (!allocate) {
    return nullptr;
  }

  /* Otherwise, we allocate a new entry */
  auto new_table = Gaia::Vm::phys_alloc(true).unwrap();

  table[index] = (uintptr_t)new_table | PTE_PRESENT | PTE_USER | PTE_WRITABLE;

  return (uint64_t *)Hal::phys_to_virt((uintptr_t)new_table);
}

static uint64_t vm_prot_to_mmu(Prot prot) {
  uint64_t ret = 0;

  if (prot & Prot::READ)
    ret |= PTE_PRESENT;

  if (prot & Prot::WRITE)
    ret |= PTE_WRITABLE;

  if (!(prot & Prot::EXECUTE))
    ret |= PTE_NOT_EXECUTABLE;

  return ret;
}

static Prot mmu_prot_to_vm(uint64_t prot) {
  Prot ret = (Prot)0;

  if (!PTE_IS_NOT_EXECUTABLE(prot))
    ret = (Prot)((int)ret | Prot::EXECUTE);

  if (PTE_IS_WRITABLE(prot))
    ret = (Prot)((int)ret | Prot::WRITE);

  if (PTE_IS_PRESENT(prot))
    ret = (Prot)((int)ret | Prot::READ);
  return ret;
}

static void pmap_fork_intern(uint64_t *dest, uint64_t *src, size_t page_count,
                             int level) {
  for (uint64_t i = 0; i < page_count; i++) {
    uint64_t orig_entry = src[i];

    if (PTE_IS_PRESENT(orig_entry)) {
      uintptr_t *entry = &dest[i];

#undef PTE_GET_ADDR
#define PTE_GET_ADDR(x) (x & 0x0000fffffffff000)

      ASSERT(!PTE_IS_PRESENT(*entry));

      memcpy(entry, &orig_entry, sizeof(uint64_t));

      auto orig_next = Hal::phys_to_virt(PTE_GET_ADDR(*entry));

      uintptr_t next_phys_addr = (uintptr_t)Gaia::Vm::phys_alloc(true).unwrap();

      *entry &= 0xffff000000000fff;
      *entry |= (next_phys_addr & 0x0000fffffffff000);

      void *next = (void *)Hal::phys_to_virt(next_phys_addr);

      if (level > 0) {
        pmap_fork_intern((uint64_t *)next, (uint64_t *)orig_next, 512,
                         level - 1);
      } else {
        memcpy(next, (void *)orig_next, 0x1000);
      }
    }
  }
}

void Pagemap::copy(Pagemap *dest) {
  (void)dest;
  pmap_fork_intern((uint64_t *)(Hal::phys_to_virt((uintptr_t)dest->context)),
                   (uint64_t *)(Hal::phys_to_virt((uintptr_t)context)), 256, 3);
}

void Pagemap::map(uintptr_t va, uintptr_t pa, Prot prot, Flags flags) {

  lock.lock();

  size_t level4 = PML_ENTRY(va, 39);
  size_t level3 = PML_ENTRY(va, 30);
  size_t level2 = PML_ENTRY(va, 21);
  size_t level1 = PML_ENTRY(va, 12);

  bool user = flags & Flags::USER;
  bool huge = flags & Flags::HUGE;
  bool large = flags & Flags::LARGE;

  auto mmu_flags = pa | vm_prot_to_mmu(prot) | (user ? PTE_USER : 0) |
                   (huge || large ? PTE_HUGE : 0);

  auto *pml3 = get_next_level((uint64_t *)Hal::phys_to_virt((uintptr_t)context),
                              level4, true);

  // If we're mapping 1G pages, we don't care about the rest of the mapping,
  // only the pml3.
  if (cpu_supports_1gb_pages && huge) {
    pml3[level3] = mmu_flags;
    lock.unlock();
    return;
  }

  // If we're mapping 2M pages, we don't care about the rest of the mapping,
  // only the pml2.
  auto *pml2 = get_next_level(pml3, level3, true);

  if (large) {
    pml2[level2] = mmu_flags;
    lock.unlock();
    return;
  }

  auto *pml1 = get_next_level(pml2, level2, true);

  pml1[level1] = mmu_flags;
  lock.unlock();
}

void Pagemap::remap(uintptr_t virt, Prot prot, Flags flags) {

  auto mapping = get_mapping(virt);

  if (mapping.is_ok()) {
    map(virt, mapping.unwrap().address, prot, flags);
    tlb_shootdown(virt);
  }
}

void Pagemap::unmap(uintptr_t va) {
  lock.lock();
  size_t level4 = PML_ENTRY(va, 39);
  size_t level3 = PML_ENTRY(va, 30);
  size_t level2 = PML_ENTRY(va, 21);
  size_t level1 = PML_ENTRY(va, 12);

  auto *pml3 = get_next_level((uint64_t *)Hal::phys_to_virt((uintptr_t)context),
                              level4, false);

  auto *pml2 = get_next_level(pml3, level3, false);
  auto *pml1 = get_next_level(pml2, level2, false);

  uint64_t *pml = nullptr;
  size_t level = 0;

  if (PTE_IS_HUGE(PTE_GET_FLAGS(pml3[level3]))) {
    pml = pml3;
    level = level3;
  } else if (PTE_IS_HUGE(PTE_GET_FLAGS(pml2[level2]))) {
    pml = pml2;
    level = level2;
  } else {
    pml = pml1;
    level = level1;
  }

  ASSERT(pml != nullptr);

  pml[level] = 0;

  tlb_shootdown(va);
  lock.unlock();
}

void Pagemap::init(bool kernel) {
  context = (Gaia::Vm::phys_alloc(true).unwrap());

  if (kernel) {
    for (int i = 256; i < 512; i++) {
      get_next_level((uint64_t *)Hal::phys_to_virt((uintptr_t)context), i,
                     true);
    }

    size_t page_size = 4096;

    uintptr_t text_start = ALIGN_DOWN((uintptr_t)text_start_addr, PAGE_SIZE);
    uintptr_t text_end = ALIGN_UP((uintptr_t)text_end_addr, PAGE_SIZE);
    uintptr_t rodata_start =
        ALIGN_DOWN((uintptr_t)rodata_start_addr, PAGE_SIZE);
    uintptr_t rodata_end = ALIGN_UP((uintptr_t)rodata_end_addr, PAGE_SIZE);
    uintptr_t data_start = ALIGN_DOWN((uintptr_t)data_start_addr, PAGE_SIZE);
    uintptr_t data_end = ALIGN_UP((uintptr_t)data_end_addr, PAGE_SIZE);
    struct limine_kernel_address_response *kaddr = kaddr_request.response;

    // Text is readable and executable, writing is not allowed.
    for (uintptr_t i = text_start; i < text_end; i += PAGE_SIZE) {
      uintptr_t phys = i - kaddr->virtual_base + kaddr->physical_base;
      map(i, phys, (Vm::Prot)(Vm::Prot::READ | Vm::Prot::EXECUTE),
          Vm::Flags::NONE);
    }

    // Read-only data is readable and NOT executable, writing is not allowed.
    for (uintptr_t i = rodata_start; i < rodata_end; i += PAGE_SIZE) {
      uintptr_t phys = i - kaddr->virtual_base + kaddr->physical_base;
      map(i, phys, Vm::Prot::READ, Vm::Flags::NONE);
    }

    // Data is readable and writable, but is NOT executable.
    for (uintptr_t i = data_start; i < data_end; i += PAGE_SIZE) {
      uintptr_t phys = i - kaddr->virtual_base + kaddr->physical_base;

      map(i, phys, (Vm::Prot)(Vm::Prot::READ | Vm::Prot::WRITE),
          Vm::Flags::NONE);
    }

    log("highest usable page is {:x}", Gaia::Vm::phys_highest_usable_page());

    Vm::Flags flags = NONE;

    if (page_size == MIB(2)) {
      flags = LARGE;
    } else if (page_size == GIB(1)) {
      flags = HUGE;
    }

    for (size_t i = 0; i < GIB(4); i += page_size) {
      map(Hal::phys_to_virt(i), i, (Vm::Prot)(Vm::Prot::READ | Vm::Prot::WRITE),
          flags);
    }

    for (size_t i = GIB(4); i < Gaia::Vm::phys_highest_usable_page();
         i += page_size) {
      map(Hal::phys_to_virt(i), i, (Vm::Prot)(Vm::Prot::READ | Vm::Prot::WRITE),
          flags);
    }

  }

  else {
    uint64_t *pml4 = (uint64_t *)Hal::phys_to_virt((uint64_t)context);
    uint64_t *kern_pml4 = (uint64_t *)Hal::phys_to_virt(
        (uint64_t)Gaia::Vm::kernel_pagemap.context);

    for (int i = 256; i < 512; i++) {
      pml4[i] = kern_pml4[i];
    }
  }
}

void Pagemap::activate() { write_cr3((uintptr_t)context); }

void Pagemap::destroy() { ::Gaia::Vm::phys_free(context); }

Result<Pagemap::Mapping, Error> Pagemap::get_mapping(uintptr_t vaddr) {
  size_t level4 = PML_ENTRY(vaddr, 39);
  size_t level3 = PML_ENTRY(vaddr, 30);
  size_t level2 = PML_ENTRY(vaddr, 21);
  size_t level1 = PML_ENTRY(vaddr, 12);

  auto *pml3 = get_next_level((uint64_t *)Hal::phys_to_virt((uintptr_t)context),
                              level4, false);

  if (!pml3)
    return Err(Error::NOT_A_DIRECTORY);

  auto *pml2 = get_next_level(pml3, level3, false);

  if (!pml2)
    return Err(Error::NOT_A_DIRECTORY);

  auto *pml1 = get_next_level(pml2, level2, false);

  if (!pml1)
    return Err(Error::NOT_A_DIRECTORY);

  if (!(PTE_GET_FLAGS(pml1[level1]) & PTE_PRESENT)) {
    return Err(Error::NOT_FOUND);
  }

  return Ok(
      Pagemap::Mapping{.prot = mmu_prot_to_vm(PTE_GET_FLAGS(pml1[level1])),
                       .address = PTE_GET_ADDR(pml1[level1])

      });
}

void init() {
  if (Cpuid::has_exfeature(Cpuid::ExFeature::PDPE1GB)) {
    log("Using 1gb pages for direct map");
    cpu_supports_1gb_pages = true;
  } else {
    log("Using 2mb pages for direct map");
  }
}

Pagemap get_current_map() { return Pagemap{(void *)read_cr3()}; }

} // namespace Gaia::Hal::Vm
