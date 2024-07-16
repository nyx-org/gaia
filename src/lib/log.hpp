/* SPDX-License-Identifier: BSD-2-Clause */
/**
 * @file log.hpp
 * @brief Logging utilities
 */
#pragma once
#include "frg/manual_box.hpp"
#include "lib/spinlock.hpp"
#include <frg/logging.hpp>
#include <frg/spinlock.hpp>
#include <frg/string.hpp>
#include <frg/string_stub.hpp>
#include <hal/hal.hpp>

namespace Gaia {

#define ASSERT(CONDITION)                                                      \
  ((CONDITION) ? (void)0                                                       \
               : panic("assertion '{}' failed at {}:{}", #CONDITION, __FILE__, \
                       __LINE__))

/// A generic sink using \ref Gaia::Hal::debug_output
class DebugSink {
public:
  void operator()(const char *str) {
    while (*str) {
      Hal::debug_output(*str++);
    }
  }
};

/**
 * @brief Returns the basename of a path
 *
 * @param path The path to use
 * @return The basename of the path
 */
constexpr const char *file_name(const char *path) {
  const char *file = path;
  while (*path) {
    if (*path++ == '/') {
      file = path;
    }
  }
  return file;
}

/// Class combining a format string and source location
struct FormatWithLocation {
  constexpr FormatWithLocation(char const *_format,
                               char const *_file = __builtin_FILE(),
                               int _line = __builtin_LINE())
      : format{_format}, file{_file}, line{_line} {}

  char const *format;
  char const *file;
  int line;
};

extern frg::stack_buffer_logger<DebugSink> logger;
extern frg::manual_box<Spinlock> log_lock;

/**
 * @brief Logs a format string
 *
 * @tparam T
 * @param fmt The format string
 * @param args The format arguments
 */
template <bool locked = true, typename... T>
void log(FormatWithLocation fmt, T... args) {

#ifndef __KERNEL__
  return;
#endif

  char file_path[256];

  // ok... this kinda sucks but hey it's pretty cool
  memcpy(file_path, fmt.file, strlen(fmt.file));

  // remove .cpp
  file_path[strlen(fmt.file) - 4] = 0;

  if (locked) {
    log_lock->lock();
  }
  logger() << frg::fmt("\x1b[32m[{:5}.{:06d}]\x1b[0m ",
                       Hal::get_time_since_boot().seconds,
                       Hal::get_time_since_boot().milliseconds)
           << frg::fmt("\x1b[33m{}:\x1b[0m ", file_name(file_path))
           << frg::fmt(fmt.format, args...) << "\n"
           << frg::endlog;
  if (locked) {
    log_lock->unlock();
  }
}

/**
 * @brief Logs a format string in red
 *
 * @tparam T
 * @param fmt The format string
 * @param args The format arguments
 */
template <bool locked = true, typename... T>
void error(FormatWithLocation fmt, T... args) {

  if (locked) {
    log_lock->lock();
  }

  char file_path[256];

  // ok... this kinda sucks but hey it's pretty cool
  memcpy(file_path, fmt.file, strlen(fmt.file));

  // remove .cpp
  file_path[strlen(fmt.file) - 4] = 0;

  logger() << frg::fmt("\x1b[31m[{:5}.{:06d}]\x1b[0m ",
                       Hal::get_time_since_boot().seconds,
                       Hal::get_time_since_boot().milliseconds)
           << frg::fmt("\x1b[33m{}:\x1b[0m ", file_name(file_path))
           << frg::fmt(fmt.format, args...) << "\n"
           << frg::endlog;
  if (locked) {
    log_lock->unlock();
  }
}

/**
 * @brief Shows a formatted message and halts the system
 *
 * @tparam T
 * @param fmt The format string
 * @param args The format arguments
 */
template <typename... T>
[[noreturn]] void panic(FormatWithLocation fmt, T... args) {
  char file_path[256];
  memcpy(file_path, fmt.file, strlen(fmt.file));
  file_path[strlen(fmt.file) - 4] = 0;
  logger() << frg::fmt("\x1b[31m[{:5}.{:06d}] PANIC \x1b[0m",
                       Hal::get_time_since_boot().seconds,
                       Hal::get_time_since_boot().milliseconds)
           << frg::fmt("\x1b[33m{}:{}\x1b[0m ", file_name(file_path), fmt.line)
           << frg::fmt(fmt.format, args...) << "\x1b[0m\n"
           << frg::endlog;
#ifdef __KERNEL__
  Hal::disable_interrupts();
#endif
  Hal::halt();
}

void log_init();

} // namespace Gaia
