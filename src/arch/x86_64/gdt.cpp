#include "vm/vm_kernel.hpp"
#include <cstdint>
#include <vm/phys.hpp>
#include <vm/vm.hpp>
#include <x86_64/gdt.hpp>

namespace Gaia::x86_64 {
struct [[gnu::packed]] GdtDescriptor {
  uint16_t limit;
  uintptr_t base;
};

struct [[gnu::packed]] GdtEntry {
  uint16_t limit0;
  uint16_t base0;
  uint8_t base1;
  uint8_t access;
  uint8_t granularity;
  uint8_t base2;
};

struct [[gnu::packed]] TssDescriptor {
  uint16_t length;
  uint16_t base0;
  uint8_t base1;
  uint8_t flags1;
  uint8_t flags2;
  uint8_t base2;
  uint32_t base3;
  uint32_t reserved;
};

struct Gdt {
  GdtEntry entries[9];
  TssDescriptor tss;
};

struct [[gnu::packed]] Tss {
  uint32_t reserved0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t reserved1;
  uint64_t ist1;
  uint64_t ist2;
  uint64_t ist3;
  uint64_t ist4;
  uint64_t ist5;
  uint64_t ist6;
  uint64_t ist7;
  uint64_t reserved2;
  uint16_t reserved3;
  uint16_t iopb_offset;
};

static Gdt gdt = {{{0x0000, 0, 0, 0x00, 0x00, 0},       // NULL
                   {0xFFFF, 0, 0, 0x9A, 0x00, 0},       // 16 Bit code
                   {0xFFFF, 0, 0, 0x92, 0x00, 0},       // 16 Bit data
                   {0xFFFF, 0, 0, 0x9A, 0xCF, 0},       // 32 Bit code
                   {0xFFFF, 0, 0, 0x92, 0xCF, 0},       // 32 Bit data
                   {0x0000, 0, 0, 0x9A, 0x20, 0},       // 64 Bit code
                   {0x0000, 0, 0, 0x92, 0x00, 0},       // 64 Bit data
                   {0x0000, 0, 0, 0xF2, 0x00, 0},       // User data
                   {0x0000, 0, 0, 0xFA, 0x20, 0}},      // User code
                  {0x0000, 0, 0, 0x89, 0x00, 0, 0, 0}}; // TSS

static GdtDescriptor gdt_pointer = {.limit = sizeof(gdt) - 1,
                                    .base = (uint64_t)&gdt};

static Tss tss = {};

extern "C" void gdt_load(GdtDescriptor *pointer);
extern "C" void tss_reload(void);

static TssDescriptor make_tss_entry(uintptr_t addr) {
  return (TssDescriptor){
      .length = sizeof(TssDescriptor),
      .base0 = (uint16_t)(addr & 0xffff),
      .base1 = (uint8_t)((addr >> 16) & 0xff),
      .flags1 = 0x89,
      .flags2 = 0,
      .base2 = (uint8_t)((addr >> 24) & 0xff),
      .base3 = (uint32_t)(addr >> 32),
      .reserved = 0,
  };
}

void gdt_init() {
  tss.iopb_offset = sizeof(Tss);
  gdt.tss = make_tss_entry((uintptr_t)&tss);

  gdt_load(&gdt_pointer);
  tss_reload();
}

void gdt_init_tss() { tss.rsp0 = (uintptr_t)Vm::vm_kernel_alloc(6) + 0x6000; }

void gdt_init_ap() {
  auto new_gdt = new Gdt;

  *new_gdt = {{{0x0000, 0, 0, 0x00, 0x00, 0},       // NULL
               {0xFFFF, 0, 0, 0x9A, 0x00, 0},       // 16 Bit code
               {0xFFFF, 0, 0, 0x92, 0x00, 0},       // 16 Bit data
               {0xFFFF, 0, 0, 0x9A, 0xCF, 0},       // 32 Bit code
               {0xFFFF, 0, 0, 0x92, 0xCF, 0},       // 32 Bit data
               {0x0000, 0, 0, 0x9A, 0x20, 0},       // 64 Bit code
               {0x0000, 0, 0, 0x92, 0x00, 0},       // 64 Bit data
               {0x0000, 0, 0, 0xF2, 0x00, 0},       // User data
               {0x0000, 0, 0, 0xFA, 0x20, 0}},      // User code
              {0x0000, 0, 0, 0x89, 0x00, 0, 0, 0}}; // TSS

  auto new_tss = new Tss();
  new_tss->iopb_offset = sizeof(Tss);
  new_tss->rsp0 = (uintptr_t)Vm::vm_kernel_alloc(6) + 0x6000;

  new_gdt->tss = make_tss_entry((uintptr_t)new_tss);

  GdtDescriptor gdt_pointer = {.limit = sizeof(*new_gdt) - 1,
                               .base = (uint64_t)new_gdt};

  gdt_load(&gdt_pointer);
  tss_reload();
}

} // namespace Gaia::x86_64
