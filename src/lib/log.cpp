#include "frg/manual_box.hpp"
#include "lib/spinlock.hpp"
#include <lib/log.hpp>

namespace Gaia {
frg::stack_buffer_logger<DebugSink> logger;
frg::manual_box<Spinlock> log_lock;

void log_init() { log_lock.initialize(); }

} // namespace Gaia
