#include "hal/hal.hpp"
#include "hal/mmu.hpp"
#include "lib/log.hpp"
#include "vm/phys.hpp"
#include <lib/base.hpp>
#include <vm/vm.hpp>

namespace Gaia::Vm {

static int curr_id = 0;

Object *Object::copy() {
  auto newobj = new Object(size, anon.amap->copy());

  refcnt++;

  // !: do not assume anon?
  newobj->anon.parent = this;

  return newobj;
}

void Object::release() {

  if (--refcnt > 0)
    return;

  // ! Check type!!
  anon.amap->release();

  // ? Is committing suicide idiomatic
  delete this;
}

void Object::retain() { refcnt++; }

bool Object::fault(Space *map, uintptr_t address, size_t off,
                   Space::FaultFlags flags, Hal::Vm::Prot prot) {

  bool write = flags & Space::WRITE;

  Anon *anon = nullptr;
  auto aent = this->anon.amap->anon_at(off);

  // If anon is a copy
  if (this->anon.parent) {

    if (aent.has_value()) {
      anon = aent.value()->anon;
      anon->lock.lock();

      // Write fault and refcnt > 1, copy
      if (write && anon->refcnt > 1) {

        auto new_anon = anon->copy();
        anon->refcnt--;
        aent.value()->anon = new_anon;

        anon->lock.unlock();

        anon = new_anon;

        map->pagemap->map(address + (off * Hal::PAGE_SIZE),
                          (uintptr_t)anon->physpage, Hal::Vm::Prot::ALL,
                          Hal::Vm::Flags::USER);

        return true;
      }

      // Is there any other way to handle this?
      else if (!write) {
        error("non-write fault on anon with refcnt>1");
        anon->lock.unlock();
        return false;
      }

      anon->lock.unlock();
      return false;
    }

    auto parent_aent = this->anon.parent->anon.amap->anon_at(off);

    if (!parent_aent.has_value()) {
      anon = new Anon(phys_alloc(true).unwrap(), off);
      this->anon.parent->anon.amap->insert_anon(anon);
    }

    anon->lock.lock();

    auto new_anon = anon->copy();

    anon->lock.unlock();

    anon = new_anon;
    this->anon.amap->insert_anon(anon);
  }

  else {
    anon = aent.has_value() ? aent.value()->anon : nullptr;

    // Allocate anon on-demand
    if (!aent.has_value()) {
      anon = new Anon(phys_alloc(true).unwrap(), off);
      this->anon.amap->insert_anon(anon);
    }

    // Anon was written to and refcnt>1, copy
    else if (anon && anon->refcnt > 1 && write) {
      anon->lock.lock();

      auto newanon = anon->copy();

      anon->refcnt--;
      anon->lock.unlock();

      anon = newanon;
      aent.value()->anon = anon;
    }
  }

  ASSERT(anon != nullptr);

  // Map the anon page with its mapping's protection
  map->pagemap->map(address + (off * Hal::PAGE_SIZE), (uintptr_t)anon->physpage,
                    (Hal::Vm::Prot)(prot), Hal::Vm::Flags::USER);

  return true;
}

Object::Object(size_t size) : refcnt(1), size(size), id(curr_id++) {
  anon.amap = new AnonMap;
  anon.parent = nullptr;
}

Object::Object(size_t size, AnonMap *amap)
    : refcnt(1), size(size), id(curr_id++) {
  anon.amap = amap;
  anon.parent = nullptr;
}

} // namespace Gaia::Vm