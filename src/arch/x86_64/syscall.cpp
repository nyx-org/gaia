#include <kernel/syscalls.hpp>
#include <x86_64/asm.hpp>
#include <x86_64/syscall.hpp>

namespace Gaia::x86_64 {

void syscall_entry(Hal::InterruptFrame *frame) {

  SyscallParams params = {frame->rdi, frame->rsi, frame->rdx, frame->r10,
                          frame->r8,  frame->r9,  frame->rip, frame};

  /*  error("\033[1;34m BEFORE \033[0m");

    log("RAX=0x{:x} RBX=0x{:x} RCX=0x{:x} RDX=0x{:x}", frame->rax, frame->rbx,
        frame->rcx, frame->rdx);
    log("RSI=0x{:x} RDI=0x{:x} RBP=0x{:x} RSP=0x{:x}", frame->rsi, frame->rdi,
        frame->rbp, frame->rsp);
    log("R8=0x{:x}  R9=0x{:x}  R10=0x{:x} R11=0x{:x}", frame->r8, frame->r9,
        frame->r10, frame->r11);
    log("R12=0x{:x} R13=0x{:x} R14=0x{:x} R15=0x{:x} RIP={:016x}", frame->r12,
        frame->r13, frame->r14, frame->r15, frame->rip);*/

  log("SYSCALL #{}", frame->rax);
  frame->rax = syscall(frame->rax, params);

  /* error("\033[1;34m AFTER \033[0m");
   log("RAX=0x{:x} RBX=0x{:x} RCX=0x{:x} RDX=0x{:x}", frame->rax, frame->rbx,
       frame->rcx, frame->rdx);
   log("RSI=0x{:x} RDI=0x{:x} RBP=0x{:x} RSP=0x{:x}", frame->rsi, frame->rdi,
       frame->rbp, frame->rsp);
   log("R8=0x{:x}  R9=0x{:x}  R10=0x{:x} R11=0x{:x}", frame->r8, frame->r9,
       frame->r10, frame->r11);
   log("R12=0x{:x} R13=0x{:x} R14=0x{:x} R15=0x{:x}", frame->r12, frame->r13,
       frame->r14, frame->r15);*/
}

extern "C" void _syscall_entry();

enum msr_registers {
  MSR_APIC = 0x1B,
  MSR_EFER = 0xC0000080,
  MSR_STAR = 0xC0000081,
  MSR_LSTAR = 0xC0000082,
  MSR_COMPAT_STAR = 0xC0000083,
  MSR_SYSCALL_FLAG_MASK = 0xC0000084,
  MSR_FS_BASE = 0xC0000100,
  MSR_GS_BASE = 0xC0000101,
  MSR_KERN_GS_BASE = 0xc0000102,
};

enum msr_efer_reg {
  EFER_ENABLE_SYSCALL = 1,
};

enum msr_star_reg {
  STAR_KCODE_OFFSET = 32,
  STAR_UCODE_OFFSET = 48,
};

void syscall_init() {
  wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_ENABLE_SYSCALL);
  wrmsr(MSR_STAR, ((uint64_t)(5 * 8) << STAR_KCODE_OFFSET) |
                      ((uint64_t)(((7 - 1) * 8) | 3) << STAR_UCODE_OFFSET));
  wrmsr(MSR_LSTAR, (uint64_t)_syscall_entry);
  wrmsr(MSR_SYSCALL_FLAG_MASK, 0xfffffffe);
}

} // namespace Gaia::x86_64
