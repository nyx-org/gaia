/* SPDX-License-Identifier: BSD-2-Clause */
#include <dev/acpi/acpi.hpp>
#include <dev/builtin.hpp>
#include <dev/console/fbconsole.hpp>
#include <dev/devkit/registry.hpp>
#include <fs/tmpfs.hpp>
#include <fs/vfs.hpp>
#include <hal/hal.hpp>
#include <kernel/elf.hpp>
#include <kernel/main.hpp>
#include <kernel/sched.hpp>
#include <lib/list.hpp>
#include <posix/errno.hpp>
#include <posix/exec.hpp>
#include <posix/fd.hpp>
#include <vm/phys.hpp>
#include <vm/vm.hpp>

using namespace Gaia;

extern bool enable_interrupts_on_other_cores;

constexpr const char *init_program = "/hello";
constexpr const char *init_argv[] = {init_program, nullptr};
constexpr const char *init_envp[] = {"SHELL=/usr/bin/bash", nullptr};

static void init_rest() {
  auto task =
      sched_new_task(sched_allocate_pid(), sched_kernel_task(), true).unwrap();

  log("Launching init program {}", init_program);
  execve(*task, init_program, (const char **)init_argv,
         (const char **)init_envp)
      .unwrap();

  sched_dequeue_and_die();
}

Result<Void, Error> Gaia::main(Charon charon) {
  static Dev::FbConsole fbconsole(charon);

  Vm::phys_init(charon);
  Vm::init();

  Dev::create_registry();
  Dev::initialize_kernel_drivers();
  Dev::AcpiPc pc(charon);

  pc.dump_tables();

  Hal::init_devices(&pc);

  pc.load_drivers();

  Fs::tmpfs_init(charon);

  Dev::system_console()->create_dev();

  log("Gaia v0.0.1 (git version {}), {} pages are available",
      __GAIA_GIT_VERSION__, Vm::phys_usable_pages());

  TRY(sched_init());

  sched_new_worker_thread("kernel init", (uintptr_t)init_rest);

  enable_interrupts_on_other_cores = true;
  Hal::enable_interrupts();
  Hal::halt();

  // Dev::system_console()->clear();

  return Ok({});
}
