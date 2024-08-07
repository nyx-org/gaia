/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include <dev/devkit/service.hpp>
#include <lai/core.h>
#include <lib/charon.hpp>

namespace Gaia::Dev {

struct AcpiTable {
  struct [[gnu::packed]] {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    uint8_t oem_table_id[8];
    uint32_t oem_revision;
    char creator_id[4];
    uint32_t creator_revision;
  } header;
};

struct [[gnu::packed]] Rsdt : AcpiTable {
  uint32_t entry[];
};

struct [[gnu::packed]] Xsdt : AcpiTable {
  uint64_t entry[];
};

struct [[gnu::packed]] Rsdp {
  char signature[8];
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt;
  uint32_t length;
  uint64_t xsdt;
  uint8_t ex_checksum;
  uint8_t reserved[3];
};

class AcpiPc : public Service {
public:
  explicit AcpiPc(Charon charon);

  const char *class_name() override { return "AcpiPc"; }
  const char *name() override { return "AcpiPc0"; }

  void dump_tables();
  void load_drivers();

  frg::optional<AcpiTable *> find_table(frg::string_view signature);

  Charon &get_charon() { return charon; }

private:
  template <typename F> void iterate(lai_nsnode_t *obj, F func);
  Rsdp *rsdp;
  AcpiTable *sdt;
  bool xsdt;
  Charon charon;
};

} // namespace Gaia::Dev