#include "hal/hal.hpp"
#include <vm/phys.hpp>
#include <vm/vm.hpp>

namespace Gaia::Vm {

AnonMap *AnonMap::copy() {
  auto new_amap = new AnonMap;

  for (auto entry : entries) {
#if VM_ENABLE_COW
    entry->anon->refcnt++;
    new_amap->insert_anon(entry->anon);
#else
    entry->anon->lock.lock();
    new_amap->insert_anon(entry->anon->copy());
    entry->anon->lock.unlock();
#endif
  }

  return new_amap;
}

void AnonMap::release() {
  for (auto entry : entries) {
    entry->anon->release();
    delete entry;
  }

  entries.reset();

  delete this;
}

frg::optional<AnonMap::Entry *> AnonMap::anon_at(uintptr_t off) {
  for (auto entry : entries) {
    if (entry->anon->offset == off)
      return entry;
  }

  return frg::null_opt;
}

void AnonMap::insert_anon(Anon *anon) {
  auto entry = new Entry;
  entry->anon = anon;
  entries.insert_tail(entry);
}

void Anon::release() {
  // If still referenced, don't free
  if (--refcnt > 0)
    return;

  phys_free(physpage);

  // ? Is committing suicide idiomatic
  delete this;
}

Anon *Anon::copy() {
  ASSERT(this->physpage != nullptr);

  auto newanon = new Anon(phys_alloc(true).unwrap(), offset);

  newanon->refcnt = 1;
  newanon->offset = offset;

  memcpy((void *)Hal::phys_to_virt((uintptr_t)newanon->physpage),
         (void *)Hal::phys_to_virt((uintptr_t)this->physpage), 0x1000);

  return newanon;
}

} // namespace Gaia::Vm