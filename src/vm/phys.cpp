/* SPDX-License-Identifier: BSD-2-Clause */

#include <frg/manual_box.hpp>
#include <lib/base.hpp>
#include <lib/freelist.hpp>
#include <vm/phys.hpp>

namespace Gaia::Vm {

static frg::manual_box<Freelist> freelist{};
static size_t usable_pages = 0;
static size_t total_pages = 0;
static uintptr_t highest_usable_page = 0;
static frg::manual_box<Spinlock> lock;

static constexpr inline const char *
mmap_entry_type_to_string(CharonMmapEntryType type) {
  switch (type) {
  case FREE:
    return "free";
  case RESERVED:
    return "reserved";
  case FRAMEBUFFER:
    return "framebuffer";
  case RECLAIMABLE:
    return "reclaimable";
  case MODULE:
    return "kernel/module";
  default:
    return "???";
  }
}

size_t phys_usable_pages() { return usable_pages; }
size_t phys_total_pages() { return total_pages; }
uintptr_t phys_highest_usable_page() { return highest_usable_page; }

void phys_init(Charon charon) {
  lock.initialize();
  freelist.initialize(Hal::PAGE_SIZE);

  for (auto entry : charon.memory_map) {
    log("memmap: [{:016x}-{:016x}] {}", entry.base, entry.base + entry.size,
        mmap_entry_type_to_string(entry.type));

    total_pages += (entry.size / Hal::PAGE_SIZE);

    if (entry.type == FREE) {
      auto *new_reg =
          reinterpret_cast<Freelist::Region *>(Hal::phys_to_virt(entry.base));

      new_reg->size = entry.size;
      usable_pages += (entry.size / Hal::PAGE_SIZE);

      freelist->add_region(new_reg);

      highest_usable_page =
          MAX(highest_usable_page, entry.base + entry.size - Hal::PAGE_SIZE);
    }
  }

  log("{} MiB of usable physical memory ({} pages)",
      (usable_pages * Hal::PAGE_SIZE) / 1024 / 1024, usable_pages);
}

Result<void *, Error> phys_alloc(bool zero) {
  void *ret = nullptr;

  lock->lock();

  auto addr = freelist->alloc();

  if (!addr.is_ok()) {
    lock->unlock();
    return Err(addr.error().value());
  }

  usable_pages--;

  lock->unlock();

  ret = reinterpret_cast<void *>(Hal::virt_to_phys(addr.value().value()));

  if (zero) {
    memset(reinterpret_cast<void *>(addr.value().value()), 0, Hal::PAGE_SIZE);
  }

  return Ok(ret);
}

void phys_free(void *page) {
  uintptr_t addr = Hal::phys_to_virt(reinterpret_cast<uintptr_t>(page));

  lock->lock();
  freelist->free(addr);
  usable_pages++;
  lock->unlock();
}

} // namespace Gaia::Vm
