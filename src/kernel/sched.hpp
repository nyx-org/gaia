// @license:bsd2
#pragma once
#include <fs/vfs.hpp>
#include <hal/int.hpp>
#include <hal/mmu.hpp>
#include <kernel/event.hpp>
#include <lib/list.hpp>
#include <posix/fd.hpp>
#include <sys/types.h>
#include <vm/vm.hpp>

// FIXME: Find an architecture-independent way of doing this
#include <x86_64/idt.hpp>

#define current_thread (curr_cpu()->curr_thread)

namespace Gaia {

constexpr auto TIME_SLICE = 5;

struct Task;

struct Cpu;

struct Thread {
  enum {
    RUNNING,
    SUSPENDED,
    EXITED,
  } state;

  frg::string_view name;

  Hal::CpuContext ctx;

  Task *task;

  Vm::Vector<Task> children;

  Cpu *cpu;

  ListNode<Thread> link; // Scheduler queue

  size_t which_event;
  bool blocked;
  bool in_fault = false;

  ~Thread();
};

struct Task {
  pid_t pid;

  ListNode<Task> link;

  Vm::Vector<Thread *> threads;
  List<Task, &Task::link> children;

  Fs::Vnode *cwd;

  Posix::Fds fds;

  Task *parent;
  Vm::Space *space;

  Event *exit_event;
  int exit_code;

  ~Task();
};

pid_t sched_allocate_pid();

Result<Thread *, Error> sched_new_thread(frg::string_view name, Task *task,
                                         Hal::CpuContext ctx, bool insert);

Result<Task *, Error> sched_new_task(pid_t pid, Task *parent, bool user);

void sched_tick(Hal::InterruptFrame *frame);

void sched_yield(bool save_ctx = true);

Result<Void, Error> sched_init();

Thread *sched_curr();

void sched_enqueue_thread(Thread *thread);
void sched_dequeue_thread(Thread *thread);

void sched_add_cpu(Cpu *cpu);

[[noreturn]] void sched_dequeue_and_die();

void sched_suspend_thread(Thread *thread);
void sched_wake_thread(Thread *thread);

void sched_send_to_death(Thread *thread);

Result<Thread *, Error> sched_new_worker_thread(frg::string_view name,
                                                uintptr_t entry_point,
                                                bool insert = true);

Task *sched_kernel_task();

} // namespace Gaia
