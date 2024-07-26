/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include <lib/base.hpp>
#include <lib/list.hpp>

namespace Gaia {

struct Dpc {
  void (*func)(void *arg);
  void *args;

  ListNode<Dpc> link;
};

enum class Ipl : uint8_t {
  // Normal execution
  ZERO = 0,

  // ?: Should we add APCs for asynchronous I/O Processing?

  // Where DPCs and the scheduler are ran
  DISPATCH = 2,

  // Where driver interrupts are ran
  DEVICE = 13,

  // Block everything
  HIGH = 15,
};

Ipl ipl();
Ipl iplx(Ipl _new);

/// Enqueues a deferred procedure call
void dpc_enqueue(Dpc *dpc);

} // namespace Gaia