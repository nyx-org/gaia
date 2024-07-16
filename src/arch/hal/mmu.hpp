/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include "frg/spinlock.hpp"
#include "lib/spinlock.hpp"
#include <lib/base.hpp>
#include <lib/error.hpp>
#include <lib/result.hpp>
#include <stdint.h>

namespace Gaia::Hal {
namespace Vm {
enum Prot {
  READ = (1 << 0),
  WRITE = (1 << 1),
  EXECUTE = (1 << 2),
  ALL = READ | WRITE | EXECUTE,
};

enum Flags {
  NONE = (1 << 0),
  HUGE = (1 << 1),
  LARGE = (1 << 2),
  USER = (1 << 3),
};

class Pagemap {
public:
  void map(uintptr_t virt, uintptr_t phys, Prot prot, Flags flags);
  void remap(uintptr_t virt, Prot prot, Flags flags);
  void unmap(uintptr_t virt);
  void activate();
  void copy(Pagemap *dest);

  struct Mapping {
    Prot prot;
    uintptr_t address;
  };

  Result<Mapping, Error> get_mapping(uintptr_t virt);

  void init(bool kernel);

  Pagemap() : context(nullptr){};

  Pagemap(void *context) : context(context){};

  void destroy();

private:
#if defined(__x86_64__) || defined(__riscv)
  void *context = nullptr;
#endif

#ifdef __aarch64__
  void *context[2];
#endif

  Spinlock lock;
};

void init();
Pagemap get_current_map();

} // namespace Vm
} // namespace Gaia::Hal
