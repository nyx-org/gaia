/* @license:bsd2 */
#pragma once
#include <frg/manual_box.hpp>
#include <frg/spinlock.hpp>
#include <lib/thread-safety.hpp>

namespace Gaia {
class CAPABILITY("spinlock") Spinlock {
public:
  Spinlock() { _lock.initialize(); }
  void lock() ACQUIRE() { _lock->lock(); }
  void unlock() RELEASE() { _lock->unlock(); }
  void construct() {
    if (!_lock.valid())
      _lock.initialize();
  }

private:
  frg::manual_box<frg::simple_spinlock> _lock;
};

} // namespace Gaia
