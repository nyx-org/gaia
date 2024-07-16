/* @license:bsd2 */
#pragma once
#include <frg/spinlock.hpp>
#include <lib/thread-safety.hpp>

namespace Gaia {
class CAPABILITY("spinlock") Spinlock {
public:
  void lock() ACQUIRE() { _lock.lock(); }
  void unlock() RELEASE() { _lock.unlock(); }

private:
  frg::simple_spinlock _lock;
};

} // namespace Gaia
