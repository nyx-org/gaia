/* SPDX-License-Identifier: BSD-2-Clause */
#include "kernel/cpu.hpp"
#include <dev/acpi/acpi.hpp>
#include <lib/log.hpp>
#include <x86_64/apic.hpp>
#include <x86_64/asm.hpp>
#include <x86_64/timer.hpp>

namespace Gaia::x86_64 {

enum LapicReg {
  ID = 0x20,
  EOI = 0x0B0,
  SPURIOUS_VECTOR = 0x0F0,
  LVT_TIMER = 0x320,
  TIMER_INIT_COUNT = 0x380,
  TIMER_CURRENT_COUNT = 0x390,
  TIMER_DIVIDE_CONFIG = 0x3E0,
  ICR0 = 0x300,
  ICR1 = 0x310,
};

enum ApicTimerDivide {
  BY_2 = 0,
  BY_4 = 1,
  BY_8 = 2,
  BY_16 = 3,
  BY_32 = 4,
  BY_64 = 5,
  BY_128 = 6,
  BY_1 = 7
};

constexpr auto LAPIC_SPURIOUS_ALL = 0xFF;
constexpr auto LAPIC_SPURIOUS_ENABLE = (1 << 8);
constexpr auto LAPIC_TIMER_IRQ = 0x20;
constexpr auto LAPIC_TIMER_PERIODIC = (1 << 17);
constexpr auto LAPIC_TIMER_MASKED = (1 << 16);

static uintptr_t lapic_address = 0;
static List<IoApic, &IoApic::link> ioapics;
static List<Iso, &Iso::link> isos;

static inline uintptr_t get_lapic_address() {
  if (!lapic_address) {
    lapic_address = rdmsr(0x1b) & 0xfffff000;
  }
  return lapic_address;
}

static void lapic_write(uint32_t reg, uint32_t value) {
  auto addr = get_lapic_address();
  volatile_write<uint32_t>(
      reinterpret_cast<uint32_t *>(Hal::phys_to_virt(addr) + reg), value);
}

static uint32_t lapic_read(uint32_t reg) {
  auto addr = get_lapic_address();
  return volatile_read<uint32_t>(
      reinterpret_cast<uint32_t *>(Hal::phys_to_virt(addr) + reg));
}

void lapic_send_ipi(uint8_t vector) {
  lapic_write(LapicReg::ICR1, (0) << 24);
  lapic_write(LapicReg::ICR0, vector);
}

void lapic_init() {
  lapic_write(LapicReg::SPURIOUS_VECTOR, lapic_read(LapicReg::SPURIOUS_VECTOR) |
                                             LAPIC_SPURIOUS_ALL |
                                             LAPIC_SPURIOUS_ENABLE);
  lapic_write(LapicReg::TIMER_DIVIDE_CONFIG, ApicTimerDivide::BY_16);
  lapic_write(LapicReg::TIMER_INIT_COUNT, -1);
  timer_sleep(10);
  lapic_write(LapicReg::LVT_TIMER, LAPIC_TIMER_MASKED);
  uint32_t ticks_in_10ms = -1 - lapic_read(LapicReg::TIMER_CURRENT_COUNT);

  lapic_write(LapicReg::LVT_TIMER, LAPIC_TIMER_IRQ | LAPIC_TIMER_PERIODIC);
  lapic_write(LapicReg::TIMER_DIVIDE_CONFIG, ApicTimerDivide::BY_16);

  /* Tick every milisecond */
  lapic_write(LapicReg::TIMER_INIT_COUNT, ticks_in_10ms / 10);
}

uint32_t IoApic::read(uint32_t reg) const {
  auto address = Hal::phys_to_virt(ioapic.address);
  volatile_write<uint32_t>(reinterpret_cast<uint32_t *>(address), reg);
  return volatile_read<uint32_t>(reinterpret_cast<uint32_t *>(address + 0x10));
}

void IoApic::write(uint32_t reg, uint32_t value) const {
  auto address = Hal::phys_to_virt(ioapic.address);
  volatile_write<uint32_t>(reinterpret_cast<uint32_t *>(address), reg);
  volatile_write<uint32_t>(reinterpret_cast<uint32_t *>(address + 0x10), value);
}

size_t IoApic::get_max_redirect() const {
  struct [[gnu::packed]] Version {
    uint8_t version;
    uint8_t reserved;
    uint8_t max_redirect;
    uint8_t reserved2;
  };

  uint32_t val = read(1);

  auto version = reinterpret_cast<Version *>(&val);

  return version->max_redirect + 1;
}

static Result<IoApic *, Error> ioapic_from_gsi(uint32_t gsi) {
  for (auto ioapic : ioapics) {
    if (gsi >= ioapic->ioapic.interrupt_base &&
        gsi < ioapic->ioapic.interrupt_base + ioapic->get_max_redirect()) {
      return Ok(ioapic);
    }
  }

  return Err(Error::NOT_FOUND);
}

static void ioapic_set_gsi_redirect(uint32_t lapic_id, uint8_t vector,
                                    uint8_t gsi, bool lopol, bool edge) {
  union [[gnu::packed]] Redirect {
    struct [[gnu::packed]] {
      uint8_t vector;
      uint8_t delivery_mode : 3;
      uint8_t dest_mode : 1;
      uint8_t delivery_status : 1;
      uint8_t polarity : 1;
      uint8_t remote_irr : 1;
      uint8_t trigger : 1;
      uint8_t mask : 1;
      uint8_t reserved : 7;
      uint8_t dest_id;
    } _redirect;

    struct [[gnu::packed]] {
      uint32_t _low_byte;
      uint32_t _high_byte;
    } _raw;
  };

  auto ioapic = ioapic_from_gsi(gsi).unwrap();

  Redirect redirect{};
  redirect._redirect.vector = vector;

  redirect._redirect.polarity = lopol;
  redirect._redirect.trigger = !edge;

  redirect._redirect.dest_id = lapic_id;

  uint32_t io_redirect_table = (gsi - ioapic->ioapic.interrupt_base) * 2 + 16;

  ioapic->write(io_redirect_table, (uint32_t)redirect._raw._low_byte);
  ioapic->write(io_redirect_table + 1, redirect._raw._high_byte);
}

static void ioapic_set_gsi_redirect(uint32_t lapic_id, uint8_t vector,
                                    uint8_t gsi, uint16_t flags) {
  union [[gnu::packed]] Redirect {
    struct [[gnu::packed]] {
      uint8_t vector;
      uint8_t delivery_mode : 3;
      uint8_t dest_mode : 1;
      uint8_t delivery_status : 1;
      uint8_t polarity : 1;
      uint8_t remote_irr : 1;
      uint8_t trigger : 1;
      uint8_t mask : 1;
      uint8_t reserved : 7;
      uint8_t dest_id;
    } _redirect;

    struct [[gnu::packed]] {
      uint32_t _low_byte;
      uint32_t _high_byte;
    } _raw;
  };

  auto ioapic = ioapic_from_gsi(gsi).unwrap();

  Redirect redirect{};
  redirect._redirect.vector = vector;

  if (flags & (1 << 1)) {
    redirect._redirect.polarity = 1;
  }

  if (flags & (1 << 2)) {
    redirect._redirect.trigger = 1;
  }

  redirect._redirect.dest_id = lapic_id;

  uint32_t io_redirect_table = (gsi - ioapic->ioapic.interrupt_base) * 2 + 16;

  ioapic->write(io_redirect_table, (uint32_t)redirect._raw._low_byte);
  ioapic->write(io_redirect_table + 1, redirect._raw._high_byte);
}

void ioapic_init(Dev::AcpiPc *pc) {
  auto madt_res = pc->find_table("APIC");

  ASSERT(madt_res.has_value() == true);

  auto madt = reinterpret_cast<Madt *>(madt_res.value());

  size_t i = 0;
  while (i < madt->header.length - sizeof(Madt)) {
    auto *entry = reinterpret_cast<MadtEntry *>(madt->entries + i);

    switch (entry->type) {
    case 1: {
      auto *new_ent = new IoApic(*reinterpret_cast<MadtIoApic *>(entry));
      ioapics.insert_tail(new_ent);
      break;
    }

    case 2: {
      auto *new_ent = new Iso;
      new_ent->iso = *reinterpret_cast<MadtIso *>(entry);
      isos.insert_tail(new_ent);
      break;
    }
    }

    i += MAX(2, entry->length);
  }

  // Redirect timer to vector 32
  ioapic_redirect_irq(0, 32);

  // keyboard
  ioapic_redirect_irq(1, 33);
}

void ioapic_handle_gsi(uint32_t gsi, Hal::InterruptHandler *handler, void *arg,
                       bool lopol, bool edge, Ipl ipl,
                       Hal::InterruptEntry *entry) {
  auto vec = Hal::allocate_interrupt(ipl, handler, arg, entry).unwrap();
  ioapic_set_gsi_redirect(curr_cpu()->num, vec, gsi, lopol, edge);
}

void ioapic_redirect_irq(uint8_t irq, uint8_t vector) {
  for (auto iso : isos) {
    if (iso->iso.irq_source == irq) {
      ioapic_set_gsi_redirect(curr_cpu()->num, vector, iso->iso.gsi,
                              iso->iso.flags);
      return;
    }
  }

  ioapic_set_gsi_redirect(curr_cpu()->num, vector, irq, 0);
}

void lapic_eoi() { lapic_write(LapicReg::EOI, 0); }

} // namespace Gaia::x86_64
