/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include <stdint.h>

namespace Gaia::x86_64 {
static inline void outb(uint16_t port, uint8_t data) {
  asm volatile("outb %0, %1" ::"a"(data), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
  uint8_t data;
  asm volatile("inb %1, %0" : "=a"(data) : "Nd"(port));
  return data;
}

static inline void outw(uint16_t port, uint16_t data) {
  asm volatile("outw %0, %1" ::"a"(data), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
  uint16_t data;
  asm volatile("inw %1, %0" : "=a"(data) : "Nd"(port));
  return data;
}

static inline void outl(uint16_t port, uint32_t data) {
  asm volatile("outl %0, %1" ::"a"(data), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
  uint32_t data;
  asm volatile("inl %1, %0" : "=a"(data) : "Nd"(port));
  return data;
}

static inline uint64_t rdtsc(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static inline void cli(void) { asm volatile("cli"); }

static inline void hlt(void) { asm volatile("hlt"); }

static inline void sti(void) { asm volatile("sti"); }

#define ASM_MAKE_CRN(N)                                                        \
  static inline uint64_t read_cr##N(void) {                                    \
    uint64_t value = 0;                                                        \
    asm volatile("mov %%cr" #N ", %0" : "=r"(value));                          \
    return value;                                                              \
  }                                                                            \
                                                                               \
  static inline void write_cr##N(uint64_t value) {                             \
    asm volatile("mov %0, %%cr" #N ::"a"(value));                              \
  }

ASM_MAKE_CRN(0)
ASM_MAKE_CRN(1)
ASM_MAKE_CRN(2)
ASM_MAKE_CRN(3)
ASM_MAKE_CRN(4)
ASM_MAKE_CRN(8)

static inline uint64_t read_xcr(uint32_t i) {
  uint32_t eax, edx;
  asm volatile("xgetbv"

               : "=a"(eax), "=d"(edx)
               : "c"(i)
               : "memory");

  return eax | ((uint64_t)edx << 32);
}

static inline void write_xcr(uint32_t i, uint64_t value) {
  uint32_t edx = value >> 32;
  uint32_t eax = (uint32_t)value;
  asm volatile("xsetbv" : : "a"(eax), "d"(edx), "c"(i) : "memory");
}

static inline uint64_t rdmsr(uint32_t msr) {
  uint32_t edx = 0, eax = 0;
  asm volatile("rdmsr\n\t" : "=a"(eax), "=d"(edx) : "c"(msr) : "memory");
  return ((uint64_t)edx << 32) | eax;
}

static inline uint64_t wrmsr(uint32_t msr, uint64_t val) {
  uint32_t eax = (uint32_t)val;
  uint32_t edx = (uint32_t)(val >> 32);
  asm volatile("wrmsr\n\t" : : "a"(eax), "d"(edx), "c"(msr) : "memory");
  return ((uint64_t)edx << 32) | eax;
}

static inline void xsave(uint8_t *region) {
  asm volatile("xsave %0" ::"m"(*region), "a"(~(uintptr_t)0), "d"(~(uintptr_t)0)
               : "memory");
}

static inline void xrstor(uint8_t *region) {
  asm volatile("xrstor %0" ::"m"(*region), "a"(~(uintptr_t)0),
               "d"(~(uintptr_t)0)
               : "memory");
}

static inline void fxsave(void *region) {
  asm volatile("fxsave (%0)" ::"a"(region));
}

static inline void fxrstor(void *region) {
  asm volatile("fxrstor (%0)" ::"a"(region));
}

static inline void fninit(void) { asm volatile("fninit"); }

static inline void set_kernel_gs_base(void *addr) {
  wrmsr(0xc0000102, (uint64_t)addr);
}

static inline void *get_kernel_gs_base() { return (void *)rdmsr(0xc0000102); }

static inline void *get_gs_base() { return (void *)rdmsr(0xc0000101); }

static inline void *get_fs_base() { return (void *)rdmsr(0xc0000100); }

static inline void set_gs_base(void *addr) {
  wrmsr(0xc0000101, (uint64_t)addr);
}

static inline void set_fs_base(void *addr) {
  wrmsr(0xc0000100, (uint64_t)addr);
}

enum Cr4Bit {
  CR4_VIRTUAL_8086_MODE_EXT = (1 << 0),
  CR4_PROTECTED_MODE_VIRTUAL_INT = (1 << 1),
  CR4_TIME_STAMP_DISABLE = (1 << 2),
  CR4_DEBUGGING_EXT = (1 << 3),
  CR4_PAGE_SIZE_EXT = (1 << 4),
  CR4_PHYSICAL_ADDRESS_EXT = (1 << 5),
  CR4_MACHINE_CHECK_EXCEPTION = (1 << 6),
  CR4_PAGE_GLOBAL_ENABLE = (1 << 7),
  CR4_PERFORMANCE_COUNTER_ENABLE = (1 << 8),
  CR4_FXSR_ENABLE = (1 << 9),
  CR4_SIMD_EXCEPTION_SUPPORT = (1 << 10),
  CR4_USER_MODE_INSTRUCTION_PREVENTION = (1 << 11),
  CR4_5_LEVEL_PAGING_ENABLE = (1 << 12),
  CR4_VIRTUAL_MACHINE_EXT_ENABLE = (1 << 13),
  CR4_SAFER_MODE_EXT_ENABLE = (1 << 14),
  CR4_FS_GS_BASE_ENABLE = (1 << 16),
  CR4_PCID_ENABLE = (1 << 17),
  CR4_XSAVE_ENABLE = (1 << 18),
  CR4_SUPERVISOR_EXE_PROTECTION_ENABLE = (1 << 20),
  CR4_SUPERVISOR_ACCESS_PROTECTION_ENABLE = (1 << 21),
  CR4_KEY_PROTECTION_ENABLE = (1 << 22),
  CR4_CONTROL_FLOW_ENABLE = (1 << 23),
  CR4_SUPERVISOR_KEY_PROTECTION_ENABLE = (1 << 24),
};

enum XCr0Bit {
  XCR0_XSAVE_SAVE_X87 = (1 << 0),
  XCR0_XSAVE_SAVE_SSE = (1 << 1),
  XCR0_AVX_ENABLE = (1 << 2),
  XCR0_BNDREG_ENABLE = (1 << 3),
  XCR0_BNDCSR_ENABLE = (1 << 4),
  XCR0_AVX512_ENABLE = (1 << 5),
  XCR0_ZMM0_15_ENABLE = (1 << 6),
  XCR0_ZMM16_32_ENABLE = (1 << 7),
  XCR0_PKRU_ENABLE = (1 << 9),
};

enum Cr0Bit {
  CR0_PROTECTED_MODE_ENABLE = (1 << 0),
  CR0_MONITOR_CO_PROCESSOR = (1 << 1),
  CR0_EMULATION = (1 << 2),
  CR0_TASK_SWITCHED = (1 << 3),
  CR0_EXTENSION_TYPE = (1 << 4),
  CR0_NUMERIC_ERROR_ENABLE = (1 << 5),
  CR0_WRITE_PROTECT_ENABLE = (1 << 16),
  CR0_ALIGNMENT_MASK = (1 << 18),
  CR0_NOT_WRITE_THROUGH_ENABLE = (1 << 29),
  CR0_CACHE_DISABLE = (1 << 30),
  CR0_PAGING_ENABLE = (1 << 31),
};
} // namespace Gaia::x86_64
