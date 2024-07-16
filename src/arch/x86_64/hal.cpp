/* SPDX-License-Identifier: BSD-2-Clause */
#include <dev/acpi/acpi.hpp>
#include <dev/console/fbconsole.hpp>
#include <hal/hal.hpp>
#include <hal/int.hpp>
#include <x86_64/apic.hpp>
#include <x86_64/asm.hpp>
#include <x86_64/cpuid.hpp>
#include <x86_64/gdt.hpp>
#include <x86_64/smp.hpp>
#include <x86_64/syscall.hpp>
#include <x86_64/timer.hpp>

using namespace Gaia::x86_64;

namespace Gaia::x86_64 {
uintptr_t get_tsc_ms();
}

namespace Gaia::Hal {
void debug_output(char c) {
  outb(0xe9, c);
  if (Dev::system_console()) {
    Dev::system_console()->log_output(c);
  }
}

[[noreturn]] void halt() {
  for (;;) {
    hlt();
  }
}

void disable_interrupts() { cli(); }
void enable_interrupts() { sti(); }

Time get_time_since_boot() {
  auto ms = get_tsc_ms();

  uint64_t seconds = ms / 1000;
  uint64_t left = ms % 1000;

  return {0, 0, seconds, left, 0};
}

uintptr_t phys_to_virt(uintptr_t phys) { return phys + 0xffff800000000000; }
uintptr_t virt_to_phys(uintptr_t virt) { return virt - 0xffff800000000000; }

const size_t PAGE_SIZE = 4096;

void init_devices(Dev::AcpiPc *pc) {
  x86_64::timer_init(pc);
  x86_64::smp_init();
  log("CPU is {}", Cpuid::branding().brand);
  x86_64::ioapic_init(pc);
}

Ipl get_ipl() { return (Ipl)read_cr8(); }

void set_ipl(Ipl ipl) { write_cr8((uint64_t)ipl); }

} // namespace Gaia::Hal
