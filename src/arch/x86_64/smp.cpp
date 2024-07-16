#include "vm/vm.hpp"
#include "x86_64/apic.hpp"
#include "x86_64/asm.hpp"
#include "x86_64/gdt.hpp"
#include "x86_64/simd.hpp"
#include "x86_64/syscall.hpp"
#include <kernel/cpu.hpp>
#include <lib/base.hpp>
#include <lib/log.hpp>
#include <x86_64/limine.h>

static volatile struct limine_smp_request smprequest = {.id =
                                                            LIMINE_SMP_REQUEST};

namespace Gaia::Hal {
void set_curr_cpu(void *cpu) { x86_64::set_gs_base(cpu); }

void *get_curr_cpu() {
  void *val;
  asm volatile("mov %%gs:0, %0" : "=r"(val));
  return val;
}
} // namespace Gaia::Hal

namespace Gaia::x86_64 {

static size_t awake_cpus = 0;

static void ap_init(struct limine_smp_info *info) {
  Vm::kernel_pagemap.activate();

  auto cpu = (Cpu *)info->extra_argument;
  cpu->self = cpu;

  Hal::set_curr_cpu(&cpu->self);

  gdt_init_ap();
  idt_init();

  syscall_init();
  lapic_init();
  simd_init();

  __atomic_add_fetch(&awake_cpus, 1, __ATOMIC_SEQ_CST);

  if (cpu->num != 0) {
    asm volatile("cli");
    for (;;) {
      asm volatile("hlt");
    }
  }
}

void smp_init() {
  auto response = smprequest.response;

  ASSERT(response != nullptr);

  auto cpus = (Cpu *)Vm::malloc(sizeof(Cpu) * response->cpu_count);

  for (size_t i = 0; i < response->cpu_count; i++) {
    auto cpu = response->cpus[i];

    auto cpu_data = &cpus[i];
    cpu_data->num = cpu->lapic_id;
    cpu->extra_argument = (uintptr_t)&cpus[i];

    if (cpu->lapic_id == response->bsp_lapic_id) {
      ap_init(cpu);
      continue;
    }

    cpu->goto_address = ap_init;
  }

  while (awake_cpus != response->cpu_count) {
    asm volatile("pause");
  }

  log("All CPUs are awake!");
}

} // namespace Gaia::x86_64