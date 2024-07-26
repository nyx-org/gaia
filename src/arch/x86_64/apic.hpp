/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once

#include <dev/acpi/acpi.hpp>
#include <hal/int.hpp>
#include <kernel/ipl.hpp>
#include <lib/list.hpp>

namespace Gaia::x86_64 {
void ioapic_init(Dev::AcpiPc *pc);

void lapic_init();
void lapic_eoi();

void lapic_send_ipi(uint8_t id, uint8_t vector);

void ioapic_handle_gsi(uint32_t gsi, Hal::InterruptHandler *handler, void *arg,
                       bool lopol, bool edge, Ipl ipl,
                       Hal::InterruptEntry *entry);

void ioapic_redirect_irq(uint8_t irq, uint8_t vector);

struct [[gnu::packed]] Madt : Dev::AcpiTable {
  uint32_t local_interrupt_controller;
  uint32_t flags;
  uint8_t entries[];
};

struct [[gnu::packed]] MadtEntry {
  uint8_t type, length;
};

struct [[gnu::packed]] MadtLapic : MadtEntry {
  uint8_t processor_id;
  uint8_t apic_id;
  uint32_t flags;
};

struct [[gnu::packed]] MadtIoApic : MadtEntry {
  uint8_t ioapic_id;
  uint8_t reserved;
  uint32_t address;
  uint32_t interrupt_base;
};

struct [[gnu::packed]] MadtIso : MadtEntry {
  uint8_t bus_source;
  uint8_t irq_source;
  uint32_t gsi;
  uint16_t flags;
};

struct IoApic {
  MadtIoApic ioapic;

  ListNode<IoApic> link;

  explicit IoApic(MadtIoApic ioapic) : ioapic(ioapic) {}

  uint32_t read(uint32_t reg) const;

  void write(uint32_t reg, uint32_t value) const;

  size_t get_max_redirect() const;
};

struct Iso {
  MadtIso iso;
  ListNode<Iso> link;
};

} // namespace Gaia::x86_64
