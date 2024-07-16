#include "kernel/cpu.hpp"
#include <lib/log.hpp>
#include <vm/heap.hpp>
#include <vm/phys.hpp>
#include <x86_64/asm.hpp>
#include <x86_64/cpuid.hpp>
#include <x86_64/simd.hpp>

static void (*simd_save)(void *) = NULL;
static void (*simd_restore)(void *) = NULL;
static size_t simd_buffer_size = 0;
static uint8_t *initial_context = NULL;

using namespace Gaia::x86_64;
using namespace Gaia;

static void simd_xsave(void *buffer) { xsave((uint8_t *)buffer); }

static void simd_xrstor(void *buffer) { xrstor((uint8_t *)buffer); }

static void simd_fxsave(void *buffer) { fxsave(buffer); }

static void simd_fxrstor(void *buffer) { fxrstor(buffer); }

namespace Gaia::x86_64 {

void simd_init(void) {
  uint64_t cr0 = read_cr0();

  cr0 &= ~((uint64_t)CR0_EMULATION);
  cr0 |= CR0_MONITOR_CO_PROCESSOR;
  cr0 |= CR0_NUMERIC_ERROR_ENABLE;

  write_cr0(cr0);
  write_cr4(read_cr4() | CR4_FXSR_ENABLE);

  write_cr4(read_cr4() | CR4_SIMD_EXCEPTION_SUPPORT);

  if (Cpuid::has_ecx_feature(Cpuid::Feature::ECX_XSAVE)) {

    if (curr_cpu()->num == 0)
      log("CPU supports xsave");
    write_cr4(read_cr4() | CR4_XSAVE_ENABLE);

    uint64_t xcr0 = 0;
    xcr0 |= XCR0_XSAVE_SAVE_X87;
    xcr0 |= XCR0_XSAVE_SAVE_SSE;
    write_xcr(0, xcr0);

    auto cpu = Cpuid::cpuid(CPUID_PROC_EXTENDED_STATE_ENUMERATION).unwrap();
    simd_save = simd_xsave;
    simd_restore = simd_xrstor;
    simd_buffer_size = cpu.ecx;
  } else {
    if (curr_cpu()->num == 0)
      log("Using legacy fxsave");
    simd_save = simd_fxsave;
    simd_restore = simd_fxrstor;
    simd_buffer_size = 512;
  }

  fninit();

  initial_context = (uint8_t *)Hal::phys_to_virt(
      (uintptr_t)::Gaia::Vm::phys_alloc(true).unwrap());
  simd_save(initial_context);
}

void simd_save_state(void *state) { simd_save(state); }

void simd_restore_state(void *state) { simd_restore(state); }

void simd_init_context(void *state) {
  memcpy(state, initial_context, simd_buffer_size);
}
} // namespace Gaia::x86_64