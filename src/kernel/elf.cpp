#include "frg/string.hpp"
#include "fs/vfs.hpp"
#include "hal/hal.hpp"
#include "hal/mmu.hpp"
#include "kernel/ipl.hpp"
#include "lib/stream.hpp"
#include "vm/vm.hpp"
#include <elf.h>
#include <kernel/elf.hpp>
#include <kernel/sched.hpp>
#include <lib/elf.hpp>
#include <vm/vm_kernel.hpp>

namespace Gaia {

struct Auxval {
  uintptr_t at_entry;
  uintptr_t at_phdr;
  uintptr_t at_phent;
  uintptr_t at_phnum;
  uintptr_t at_random;
  uintptr_t at_base;
};

Result<uintptr_t, Error> elf_load(Task &task, Fs::Vnode vnode, Auxval &auxval,
                                  uintptr_t base = 0, char *ld_path = nullptr) {
  auto stream = Fs::VnodeStream(&vnode);

  auto elf = TRY(Elf::parse(stream));
  auto header = elf.ehdr();

  if (elf.abi() != ELFOSABI_SYSV) {
    return Err(Error::INVALID_FILE);
  }

  auxval.at_phnum = header.e_phnum;
  auxval.at_phent = header.e_phentsize;
  auxval.at_base = base;

  auto phdrs = TRY(elf.phdrs());

  for (auto phdr : phdrs) {
    auto buf = new uint8_t[phdr.p_filesz];

    auto prev = TRY(stream.seek(0, Stream::Whence::CURRENT));

    TRY(stream.seek(phdr.p_offset, Stream::Whence::SET));
    TRY(stream.read(buf, phdr.p_filesz));
    TRY(stream.seek(prev, Stream::Whence::SET));

    bool has_pt_phdr = false;

    switch (phdr.p_type) {
    case PT_LOAD: {

      size_t misalign = phdr.p_vaddr & (4096 - 1);
      size_t page_count = DIV_CEIL(misalign + phdr.p_memsz, 4096);

      auto addr = base + phdr.p_vaddr;

      addr = task.space
                 ->new_anon(addr, page_count * 0x1000,
                            (Hal::Vm::Prot)((int)Hal::Vm::Prot::READ |
                                            Hal::Vm::Prot::WRITE |
                                            Hal::Vm::Prot::EXECUTE))
                 .unwrap();

      auto _ipl = iplx(Ipl::HIGH);
      for (size_t i = 0; i < page_count; i++) {
        task.space->fault(addr + i * 0x1000, Vm::Space::WRITE);
      }

      auto prev_pagemap = Hal::Vm::get_current_map();
      task.space->activate();
      memcpy((void *)addr, buf, phdr.p_filesz);
      prev_pagemap.activate();

      iplx(_ipl);

      if (!has_pt_phdr) {
        // manually figure out where the table is, this is needed for linux
        // executables
        if (header.e_phoff >= phdr.p_offset &&
            header.e_phoff < (phdr.p_offset + phdr.p_filesz)) {
          auxval.at_phdr = header.e_phoff - phdr.p_offset + phdr.p_vaddr;
        }
      }

      break;
    }

    case PT_PHDR:
      has_pt_phdr = true;
      auxval.at_phdr = base + phdr.p_vaddr;
      break;

    case PT_INTERP: {
      if (!ld_path)
        break;

      memcpy(ld_path, buf, phdr.p_filesz);

      (ld_path)[phdr.p_filesz] = 0;
      break;
    }
    }

    Vm::free(buf);
  }

  auxval.at_entry = base + header.e_entry;

  return Ok(auxval.at_entry);
}

Result<Void, Error> exec(Task &task, const char *path) {

  auto file = TRY(Fs::vfs_find(path));
  auto auxval = Auxval{};
  auto entry = TRY(elf_load(task, *file, auxval));

  task.space->new_anon(
      USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_SIZE,
      (Hal::Vm::Prot)((int)Hal::Vm::Prot::READ | Hal::Vm::Prot::WRITE));

  auto kstack =
      (uintptr_t)Vm::vm_kernel_alloc(KERNEL_STACK_SIZE / Hal::PAGE_SIZE) +
      KERNEL_STACK_SIZE;

  Hal::CpuContext ctx{entry, kstack, static_cast<uintptr_t>(USER_STACK_TOP),
                      true};

  sched_new_thread(path, &task, ctx, true);

  return Ok({});
}

// adapted from managarm
template <typename T, size_t N>
static void *push_array_to_stack(uint8_t *stack, size_t &offset,
                                 const T (&value)[N]) {
  offset -= sizeof(T) * N;
  offset -= offset & (alignof(T) - 1);
  void *ptr = (char *)stack + offset;
  memcpy(ptr, &value, sizeof(T) * N);
  return ptr;
}

Result<Void, Error> execve(Task &task, const char *path, char const *argv[],
                           char const *envp[]) {

  auto file = TRY(Fs::vfs_find(path));
  char ld_path[255] = {0};
  auto auxval = Auxval{};
  auto entry = TRY(elf_load(task, *file, auxval, 0, (char *)ld_path));

  struct stack_string {
    frg::string_view str;
    uintptr_t addr;
  };

  Vm::Vector<stack_string> args;
  Vm::Vector<stack_string> env;

  constexpr auto stack_base = USER_STACK_TOP - USER_STACK_SIZE;

  auto strings_to_vec = [](Vm::Vector<stack_string> &vec, char const *list[]) {
    for (char **elem = (char **)list; *elem; elem++) {
      vec.push_back(stack_string{.str = frg::string_view(*elem), .addr = 0});
    }
  };

  strings_to_vec(args, argv);
  strings_to_vec(env, envp);

  auto string_bytes_length = 0;
  for (auto str : args) {
    string_bytes_length += str.str.size() + 1;
  }
  for (auto str : env) {
    string_bytes_length += str.str.size() + 1;
  }

  size_t offset = USER_STACK_SIZE;

  auto mapped_stack_length =
      ALIGN_UP(string_bytes_length + sizeof(Auxval), Hal::PAGE_SIZE);
  auto mapped_stack = USER_STACK_TOP - mapped_stack_length;

  task.space->new_anon(
      mapped_stack, mapped_stack_length,
      (Hal::Vm::Prot)((int)Hal::Vm::Prot::READ | Hal::Vm::Prot::WRITE));

  // hack
  ASSERT(task.space->fault(mapped_stack, (Vm::Space::WRITE)));

  task.space->new_anon(
      stack_base, USER_STACK_SIZE - mapped_stack_length,
      (Hal::Vm::Prot)((int)Hal::Vm::Prot::READ | Hal::Vm::Prot::WRITE));

  auto _ipl = iplx(Ipl::HIGH);
  // Temporarily switch to the task's address space so we can write to the stack
  auto prev_space = Hal::Vm::get_current_map();
  task.space->activate();

  auto push_string = [&](stack_string &arg) {
    offset -= arg.str.size() + 1;
    arg.addr = stack_base + offset;
    memcpy((char *)stack_base + offset, arg.str.data(), arg.str.size() + 1);
  };

  auto push_word = [&](uintptr_t w) {
    offset -= sizeof(uintptr_t);
    memcpy((char *)stack_base + offset, &w, sizeof(uintptr_t));
  };

  for (auto &var : env) {
    push_string(var);
  }

  for (auto &arg : args) {
    push_string(arg);
  }

  offset -= offset & 15;

  if ((1 + args.size() + 1 + env.size() + 1) & 1)
    push_word(0);

  push_array_to_stack((uint8_t *)stack_base, offset,
                      (uintptr_t[]){
                          AT_ENTRY,
                          auxval.at_entry,
                          AT_PHDR,
                          auxval.at_phdr,
                          AT_PHENT,
                          auxval.at_phent,
                          AT_PHNUM,
                          auxval.at_phnum,
                          AT_BASE,
                          auxval.at_base,
                          AT_NULL,
                          0,
                      });

  push_word(0);

  // Stack is backwards, so push backwards to ensure array is contiguous
  for (size_t i = env.size(); i > 0; i--) {
    push_word(env[i - 1].addr);
  }

  push_word(0);

  // Stack is backwards, so push backwards to ensure array is contiguous
  for (size_t i = args.size(); i > 0; i--) {
    push_word(args[i - 1].addr);
  }

  push_word(args.size());

  prev_space.activate();

  iplx(_ipl);

  auto kstack =
      (uintptr_t)Vm::vm_kernel_alloc((KERNEL_STACK_SIZE / Hal::PAGE_SIZE)) +
      KERNEL_STACK_SIZE;

  Hal::CpuContext ctx{entry, kstack,
                      static_cast<uintptr_t>(stack_base + offset), true};

  if (ld_path[0]) {
    auto ld_file = TRY(Fs::vfs_find(ld_path));

    auto ld_auxval = Auxval{};
    TRY(elf_load(task, *ld_file, ld_auxval, 0x40000000, nullptr));

    ctx.regs.rip = ld_auxval.at_entry;
  }

  sched_new_thread(path, &task, ctx, true);

  return Ok({});
}

} // namespace Gaia
