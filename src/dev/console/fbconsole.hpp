/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include "flanterm.h"
#include "lib/charon.hpp"
#include "posix/tty.hpp"
#include <dev/devkit/service.hpp>

namespace Gaia::Dev {

class FbConsole : public Service {
public:
  explicit FbConsole(Charon charon);
  FbConsole() {}

  void start(Service *provider) override;
  void puts(const char *s);
  void puts(const char *s, size_t n);
  void log_output(char c);
  void input(char c);
  void clear();

  void create_dev();

  const char *class_name() override { return "FbConsole"; }
  const char *name() override { return "FbConsole"; }

private:
  flanterm_context *ctx;
  CharonFramebuffer fb;
  bool enable_log;
  friend class FbOps;

  Posix::TTY *tty;
};

FbConsole *system_console();

} // namespace Gaia::Dev