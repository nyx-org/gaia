#include <frg/manual_box.hpp>
#include <hal/int.hpp>
#include <kernel/ipl.hpp>
#include <kernel/main.hpp>
#include <kernel/sched.hpp>
#include <x86_64/apic.hpp>
#include <x86_64/asm.hpp>
#include <x86_64/idt.hpp>

namespace Gaia::x86_64 {

static List<Hal::InterruptEntry, &Hal::InterruptEntry::link> handlers[256];

/* Faster dispatching this way */
extern "C" uint64_t intr_timer_handler(uint64_t rsp) {
  auto _ipl = iplx(Ipl::HIGH);
  sched_tick((Hal::InterruptFrame *)rsp);
  lapic_eoi();
  iplx(_ipl);

  return rsp;
}

void fault() { asm volatile(""); }

extern "C" uint64_t interrupts_handler(uint64_t rsp) {
  auto stack_frame = (Hal::InterruptFrame *)rsp;
  auto _ipl = ipl();
  bool should_panic = true;

  // if pagefault, try resolving it
  if (stack_frame->intno == 0xe && sched_curr()) {

    auto space = sched_curr()->task->space;

    sched_curr()->in_fault = true;

    should_panic =
        !space->fault(read_cr2(), (Vm::Space::FaultFlags)stack_frame->err);

    if (!should_panic) {
      sched_curr()->in_fault = false;
    }
  }

  if (stack_frame->intno < 32 && should_panic) {

    auto frame = stack_frame;

    fault();

    error<false>("exception: 0x{:x}, err=0x{:x}", frame->intno, frame->err);
    error<false>("RAX=0x{:x} RBX=0x{:x} RCX=0x{:x} RDX=0x{:x}", frame->rax,
                 frame->rbx, frame->rcx, frame->rdx);
    error<false>("RSI=0x{:x} RDI=0x{:x} RBP=0x{:x} RSP=0x{:x}", frame->rsi,
                 frame->rdi, frame->rbp, frame->rsp);
    error<false>("R8=0x{:x}  R9=0x{:x}  R10=0x{:x} R11=0x{:x}", frame->r8,
                 frame->r9, frame->r10, frame->r11);
    error<false>("R12=0x{:x} R13=0x{:x} R14=0x{:x} R15=0x{:x}", frame->r12,
                 frame->r13, frame->r14, frame->r15);
    error<false>("CR0=0x{:x} CR2=0x{:x} CR3=0x{:x} RIP=\x1b[1;31m0x{:x}\x1b[0m",
                 read_cr0(), read_cr2(), read_cr3(), frame->rip);

    if (sched_curr()) {
      error<false>("In thread {} (pid={})", sched_curr()->name,
                   sched_curr()->task->pid);
    }
#if 0
    error("Backtrace: ");

    auto rbp = (uint64_t *)frame->rbp;

    while (rbp) {
      auto new_rbp = *rbp++;
      auto rip = *rbp;

      error(" - {:016x}", rip);

      rbp = (uint64_t *)new_rbp;
    }
#endif
    panic("If this wasn't intentional, please report an issue at "
          "https://github.com/nyx-org/nyx");
  }

  auto entries = &handlers[stack_frame->intno];

  if (!entries->head()) {

    if (stack_frame->intno >= 32)
      lapic_eoi();

    iplx(_ipl);
    return rsp;
  }

  for (auto entry : *entries) {
    _ipl = MAX(_ipl, entry->ipl);
  }

  _ipl = iplx(_ipl);

  for (auto entry : *entries) {
    entry->handler(stack_frame, entry->arg);
  }

  if (stack_frame->intno >= 32) {
    lapic_eoi();
  }

  iplx(_ipl);
  return rsp;
}

} // namespace Gaia::x86_64

namespace Gaia::Hal {
Result<uint8_t, Error> allocate_interrupt(Ipl ipl,
                                          Hal::InterruptHandler *handler,
                                          void *arg,
                                          Hal::InterruptEntry *entry) {
  uint8_t starting = MAX((uint64_t)ipl << 4, 32);

  for (int i = starting; i < starting + 16; i++) {
    auto slot = x86_64::handlers[i].head();
    if (!slot) {
      entry->ipl = ipl;
      entry->handler = handler;
      entry->arg = arg;

      x86_64::handlers[i].insert_tail(entry).unwrap();
      return Ok((uint8_t)i);
    }
  }

  return Err(Error::NOT_FOUND);
}

Result<uint8_t, Error>
register_interrupt_handler(int vector, Hal::InterruptHandler *handler,
                           void *arg, Hal::InterruptEntry *entry) {
  auto slot = x86_64::handlers[vector].head();
  if (!slot) {
    entry->ipl = Ipl::ZERO;
    entry->handler = handler;
    entry->arg = arg;

    x86_64::handlers[vector].insert_tail(entry).unwrap();
    return Ok((uint8_t)vector);
  }

  return Err(Error::NOT_FOUND);
}

} // namespace Gaia::Hal
