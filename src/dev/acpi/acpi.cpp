/* SPDX-License-Identifier: BSD-2-Clause */
#include <dev/acpi/acpi.hpp>
#include <dev/acpi/device.hpp>
#include <dev/acpi/glue.hpp>
#include <dev/console/fbconsole.hpp>
#include <dev/devkit/registry.hpp>
#include <lai/helpers/sci.h>

namespace Gaia::Dev {

AcpiPc::AcpiPc(Charon charon) : charon(charon) {
  rsdp = reinterpret_cast<Rsdp *>(charon.rsdp);

  if (rsdp->revision) {
    sdt = reinterpret_cast<Xsdt *>(Hal::phys_to_virt(rsdp->xsdt));
    xsdt = true;
  } else {
    sdt = reinterpret_cast<Rsdt *>(Hal::phys_to_virt(rsdp->rsdt));
    xsdt = false;
  }

  lai_glue_init((uintptr_t)rsdp, sdt, xsdt);
  lai_set_acpi_revision(rsdp->revision);
  lai_create_namespace();
  lai_enable_acpi(1);

  get_registry().set_root(this);
}

void AcpiPc::load_drivers() {

  if (charon.framebuffer.present) {
    auto fb = new FbConsole();
    fb->start(this);
  }

  // iterate the _SB node: the system bus
  iterate(lai_resolve_path(NULL, "_SB"), [this](lai_nsnode_t *obj) {
    LAI_CLEANUP_VAR lai_variable_t id = {};
    LAI_CLEANUP_STATE lai_state_t state;

    lai_init_state(&state);

    lai_nsnode_t *hid_handle = lai_resolve_path(obj, "_HID");
    if (hid_handle) {
      if (lai_eval(&id, hid_handle, &state)) {
        lai_warn("could not evaluate _HID of device");
      } else {
        LAI_ENSURE(id.type);
      }
    }

    if (!id.type) {
      lai_nsnode_t *cid_handle = lai_resolve_path(obj, "_CID");

      if (cid_handle) {
        if (lai_eval(&id, cid_handle, &state)) {
          lai_warn("could not evaluate _CID of device");
        } else {
          LAI_ENSURE(id.type);
        }
      }
    }

    if (!id.type)
      return;

    auto new_device = new AcpiDevice(id, obj);
    new_device->start(this);
  });

  log("ACPI enum complete");
}

frg::optional<AcpiTable *> AcpiPc::find_table(frg::string_view signature) {
  size_t entry_count = (sdt->header.length - sizeof(AcpiTable)) /
                       (!xsdt ? sizeof(uint32_t) : sizeof(uint64_t));
  for (size_t i = 0; i < entry_count; i++) {
    auto *entry = reinterpret_cast<AcpiTable *>(Hal::phys_to_virt(
        !xsdt ? ((Rsdt *)sdt)->entry[i] : ((Xsdt *)sdt)->entry[i]));

    if (frg::string_view(entry->header.signature, 4) == signature) {
      return entry;
    }
  }

  return frg::null_opt;
}

template <typename F> void AcpiPc::iterate(lai_nsnode_t *obj, F func) {
  struct lai_ns_child_iterator iterator =
      LAI_NS_CHILD_ITERATOR_INITIALIZER(obj);
  lai_nsnode_t *node;

  while ((node = lai_ns_child_iterate(&iterator))) {
    if (node->type != 6)
      continue;

    func(node);

    if (node->children.num_elems)
      iterate(node, func);
  }
}

void AcpiPc::dump_tables() {
  size_t entry_count = (sdt->header.length - sizeof(AcpiTable)) /
                       (!xsdt ? sizeof(uint32_t) : sizeof(uint64_t));

  auto dump_table = [](AcpiTable *table) {
    auto header = table->header;
    log("{} {:016x} ({} {} {} {}) checksum={:x} rev={}",
        frg::string_view(header.signature, 4),
        (Hal::virt_to_phys((uintptr_t)table)), header.oem_id,
        header.oem_revision, header.creator_id, header.oem_revision,
        header.checksum, header.revision);
  };

  auto header = *rsdp;
  log("{} {:016x} ({}) checksum={:x} rev={}",
      frg::string_view(header.signature, 8),
      (Hal::virt_to_phys((uintptr_t)rsdp)), header.oem_id, header.checksum,
      header.revision);
  dump_table(xsdt ? (AcpiTable *)((Xsdt *)sdt) : (AcpiTable *)((Rsdt *)sdt));

  for (size_t i = 0; i < entry_count; i++) {
    auto *entry = reinterpret_cast<AcpiTable *>(Hal::phys_to_virt(
        !xsdt ? ((Rsdt *)sdt)->entry[i] : ((Xsdt *)sdt)->entry[i]));

    dump_table(entry);
  }
}

} // namespace Gaia::Dev
