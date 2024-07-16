/* @license:bsd2 */
#pragma once

#include <hal/cpu.hpp>
#include <x86_64/smp.hpp>

namespace Gaia {

struct Cpu {
  Hal::CpuData cpu;
  int num;
  Cpu *self;
};

inline Cpu *curr_cpu() { return (Cpu *)Hal::get_curr_cpu(); }

} // namespace Gaia