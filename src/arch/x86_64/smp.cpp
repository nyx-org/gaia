#include "hal/int.hpp"
#include "hal/mmu.hpp"
#include "vm/vm.hpp"
#include "x86_64/apic.hpp"
#include "x86_64/asm.hpp"
#include "x86_64/gdt.hpp"
#include "x86_64/idt.hpp"
#include "x86_64/simd.hpp"
#include "x86_64/syscall.hpp"
#include <kernel/cpu.hpp>
#include <lib/base.hpp>
#include <lib/log.hpp>
#include <x86_64/limine.h>

static volatile struct limine_smp_request smprequest = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0,
    .response = nullptr,
    .flags = 0,
};

bool enable_interrupts_on_other_cores = false;
static size_t ncpus = 0;
static Gaia::Cpu *cpus;

namespace Gaia::Hal {
void set_curr_cpu(void *cpu) {
  log("Setting cpu to {:x}", cpu);
  x86_64::set_gs_base(cpu);
}

void *get_curr_cpu() {

  ASSERT(x86_64::get_gs_base() != nullptr);

  void *val;

  asm volatile("mov %%gs:0, %0" : "=r"(val));
  return val;
}

uintptr_t invlpg_addr = 0;
volatile uint32_t invlpg_done = 0;

static void tlb_shootdown_handler(InterruptFrame *, void *) {
  asm volatile("invlpg %0" : : "m"(*((const char *)invlpg_addr)) : "memory");
  __atomic_add_fetch(&invlpg_done, 1, __ATOMIC_RELEASE);
}

void Hal::Vm::tlb_shootdown(uintptr_t addr) {
  invlpg_addr = addr;
  __atomic_store_n(&invlpg_done, 1, __ATOMIC_RELEASE);

  for (size_t i = 0; i < ncpus; i++) {
    auto *cpu = &cpus[i];
    if (cpu->num == curr_cpu()->num)
      continue;

    x86_64::lapic_send_ipi(cpu->num, TLB_SHOOTDOWN_IPI);
  }

  while (__atomic_load_n(&invlpg_done, __ATOMIC_ACQUIRE) != ncpus)
    __asm__("pause");

  asm volatile("invlpg %0" : : "m"(*((const char *)addr)) : "memory");
}

} // namespace Gaia::Hal

namespace Gaia::x86_64 {

static size_t awake_cpus = 0;

static void ap_init(struct limine_smp_info *info) {
  Vm::kernel_pagemap.activate();
  auto cpu = (Cpu *)info->extra_argument;
  cpu->self = cpu;
  cpu->curr_thread = nullptr;

  Hal::set_curr_cpu(&cpu->self);

  gdt_init_ap();
  idt_init();

  syscall_init();
  lapic_init();
  simd_init();

  auto entry = new Hal::InterruptEntry;

  Hal::register_interrupt_handler(
      TLB_SHOOTDOWN_IPI,
      (Hal::InterruptHandler *)::Gaia::Hal::tlb_shootdown_handler, nullptr,
      entry);

  __atomic_add_fetch(&awake_cpus, 1, __ATOMIC_SEQ_CST);

  if (cpu->num != 0) {

    // this sucks
    while (__atomic_load_n(&enable_interrupts_on_other_cores,
                           __ATOMIC_SEQ_CST) != true) {
      asm volatile("pause");
    }

    asm volatile("sti");
    for (;;) {
      asm volatile("hlt");
    }
  }
}

void smp_init() {
  auto response = smprequest.response;

  ASSERT(response != nullptr);

  cpus = (Cpu *)Vm::malloc(sizeof(Cpu) * response->cpu_count);

  ncpus = response->cpu_count;

  for (size_t i = 0; i < response->cpu_count; i++) {
    new (&cpus[i]) Cpu();

    auto cpu = response->cpus[i];

    auto cpu_data = &cpus[i];
    cpu_data->num = cpu->lapic_id;
    cpu->extra_argument = (uintptr_t)&cpus[i];

    sched_add_cpu(&cpus[i]);

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