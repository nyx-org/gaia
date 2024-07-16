/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once

#include <asm-generic/termbits.h>
#include <fs/devfs.hpp>
#include <kernel/event.hpp>

namespace Gaia::Posix {

class TTY {
public:
  TTY(size_t rows, size_t cols) : rows(rows), cols(cols) {
    termios.c_cc[VEOL] = '\n';
    termios.c_cc[VEOF] = 0;
    termios.c_cc[VERASE] = '\b';
    termios.c_cflag = (CREAD | CS8 | HUPCL);
    termios.c_iflag = (BRKINT | ICRNL | IMAXBEL | IXON | IXANY);
    termios.c_lflag = (ECHO | ICANON | ISIG | IEXTEN | ECHOE);
    termios.c_oflag = (OPOST | ONLCR);
  };

  static void register_tty(TTY *tty, dev_t minor);

  using WriteCallback = void(char c, void *context);

  void input(char c) REQUIRES(lock);

  void set_write_callback(WriteCallback *callback, void *context) {
    this->callback = callback;
    callback_context = context;
  }

  class Ops : public Fs::DevOps {
    Result<size_t, Error> write(dev_t minor, frg::span<uint8_t> buf,
                                off_t off) override;

    Result<size_t, Error> read(dev_t minor, frg::span<uint8_t> buf,
                               off_t off) override;

    Result<uint64_t, Error> ioctl(dev_t minor, uint64_t request,
                                  void *arg) override;

    Result<Fs::VnodeAttr, Error> getattr(dev_t minor) override;
  } ops;

private:
  // Ringbuffer
  frg::array<char, 4096> buffer;
  size_t buf_length, write_cursor, read_cursor;
  size_t rows, cols;
  Spinlock lock;

  struct termios termios;
  Event read_event;

  void push(char c);
  char pop();
  char erase();

  WriteCallback *callback;
  void *callback_context;

  inline bool is_cflag_set(int flag) { return termios.c_cflag & flag; }
  inline bool is_iflag_set(int flag) { return termios.c_iflag & flag; }
  inline bool is_oflag_set(int flag) { return termios.c_oflag & flag; }
  inline bool is_lflag_set(int flag) { return termios.c_lflag & flag; }
};

} // namespace Gaia::Posix