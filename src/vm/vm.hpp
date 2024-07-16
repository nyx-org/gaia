/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include "frg/manual_box.hpp"
#include "lib/list.hpp"
#include <hal/hal.hpp>
#include <hal/mmu.hpp>
#include <lib/base.hpp>
#include <vm/vmem.h>

#define VM_ENABLE_COW 1

namespace Gaia::Vm {
void init();

extern Hal::Vm::Pagemap kernel_pagemap;

struct Object;

class Space {
public:
  Result<uintptr_t, Error> map(Object *obj, frg::optional<uintptr_t> address,
                               size_t size, Hal::Vm::Prot prot,
                               bool lazy = true);

  Result<Void, Error> unmap(uintptr_t address, size_t size);

  // Matches i386 mmu
  enum FaultFlags {
    PRESENT = (1 << 0),
    WRITE = (1 << 1),
    USER = (1 << 2),
    EXEC = (1 << 4),
  };

  // Fault on address, returns true if fault was resolved
  bool fault(uintptr_t address, FaultFlags flags);

  Result<uintptr_t, Error> new_anon(frg::optional<uintptr_t> address,
                                    size_t size, Hal::Vm::Prot prot,
                                    bool lazy = true);

  Result<Void, Error> copy(Space *dest);

  void activate() { pagemap->activate(); }

  Space(Hal::Vm::Pagemap *pagemap, Vmem vmem) : pagemap(pagemap), vmem(vmem) {}

  Space(const char *name, bool user) {
    pagemap = new Hal::Vm::Pagemap();
    pagemap->init(!user);

    vmem_init(&vmem, name, (void *)0x80000000000, 0x100000000, 0x1000, 0, 0, 0,
              0, 0);
  }

  void release();

  Hal::Vm::Pagemap *pagemap;

private:
  struct Entry {
    ListNode<Entry> link;
    uintptr_t start;
    size_t size;
    Object *obj;
    Hal::Vm::Prot prot;

    Entry(uintptr_t start, size_t size, Object *obj, Hal::Vm::Prot prot)
        : start(start), size(size), obj(obj), prot(prot) {}
  };

  List<Entry, &Entry::link> entries;

  Vmem vmem;
};

class AnonMap;

struct Anon;

struct Object {
  int refcnt;
  size_t size;
  int id;

  union {
    struct {
      AnonMap *amap;
      Object *parent; // if copied-on-write, original parent
    } anon;
  };

  // Create a (CoW-optimized) copy of the object
  Object *copy();

  void retain();

  // Release the object
  // NOTE: if the refcnt reaches 0, object WILL commit suicide (`delete this`)
  void release();

  Object(size_t size);
  Object(size_t size, AnonMap *amap);

  // Try and resolve a fault on the object
  bool fault(Space *map, uintptr_t vaddr, size_t offset,
             Space::FaultFlags flags, Hal::Vm::Prot obj_prot);
};

struct Anon {
  int refcnt;
  void *physpage;
  size_t offset; // offset within the amap
  bool lazy;
  Spinlock lock;

  void release();
  Anon *copy() REQUIRES(lock);
  Anon(void *physpage, size_t offset)
      : refcnt(1), physpage(physpage), offset(offset){};
};

class AnonMap {
public:
  struct Entry {
    Anon *anon;
    ListNode<Entry> link;
  };

  frg::optional<Entry *> anon_at(uintptr_t page);

  void insert_anon(Anon *anon);

  AnonMap *copy();

  void release();

private:
  List<Entry, &Entry::link> entries;
};

struct Page {
  Hal::Vm::Pagemap *map;
  uintptr_t virt, phys;
  ListNode<Page> link;
};

extern frg::manual_box<List<Page, &Page::link>> pages_list;

} // namespace Gaia::Vm
