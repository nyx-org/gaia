/* SPDX-License-Identifier: BSD-2-Clause */

#include <hal/int.hpp>
#include <kernel/ipl.hpp>
#include <lib/list.hpp>
#include <lib/queue.h>

namespace Gaia {

static List<Dpc, &Dpc::link> dpc_queue;

static void dispatch_dpc() {
  auto _ipl = iplx(Ipl::DISPATCH);

  auto dpc = dpc_queue.head();

  while (dpc) {
    dpc_queue.remove(dpc);

    ASSERT(dpc->func != nullptr);
    dpc->func(dpc->args);

    dpc = dpc_queue.head();
  }

  iplx(_ipl);
}

static void ipl_lowered(Ipl to) {
  (void)to;
  (void)dispatch_dpc;
  // if ipl was lowered below DISPATCH_LEVEL, run DPCs
  /*if ((uint64_t)to < (uint64_t)Ipl::DISPATCH) {
    dispatch_dpc();
  }*/
}

Ipl ipl() { return Hal::get_ipl(); }

Ipl iplx(Ipl _new) {
  auto old = ipl();
  Hal::set_ipl(_new);

  // If ipl was lowered
  if ((uint64_t)_new < (uint64_t)old) {
    ipl_lowered(_new);
  }

  return old;
}

void dpc_enqueue(Dpc *dpc) {
  if (ipl() < Ipl::DISPATCH) {
    auto _ipl = iplx(Ipl::DISPATCH);
    dpc->func(dpc->args);
    iplx(_ipl);
    return;
  }

  dpc_queue.insert_head(dpc);
}

} // namespace Gaia