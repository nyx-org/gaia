#include <lib/spinlock.hpp>

namespace Gaia {

void Spinlock::lock() ACQUIRE() { _lock->lock(); }

void Spinlock::unlock() RELEASE() { _lock->unlock(); }

} // namespace Gaia