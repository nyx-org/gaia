/* @license:bsd2 */
#pragma once

#include <hal/cpu.hpp>
#include <kernel/sched.hpp>
#include <x86_64/smp.hpp>

namespace Gaia {

struct Cpu {
  Hal::CpuData cpu;
  int num;
  Cpu *self;
  uint32_t other_magic = 0xB00B135;
  Thread *curr_thread, *idle_thread;
  List<Thread, &Thread::link> runqueue;
  uint32_t magic = 0xCAFEBABE;
};

inline Cpu *curr_cpu() { return (Cpu *)Hal::get_curr_cpu(); }

} // namespace Gaia