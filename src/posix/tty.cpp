#include <asm-generic/ioctls.h>
#include <fs/vfs.hpp>
#include <posix/tty.hpp>
#include <sys/stat.h>

namespace Gaia::Posix {

// ? Not sure about locks here.. rework when implementing SMP.

static TTY *ttys[1];

void TTY::register_tty(TTY *tty, dev_t minor) {
  ASSERT(minor == 0);
  ttys[minor] = tty;
}

void TTY::push(char c) {
  lock.lock();

  if (buf_length == buffer.size()) {
    lock.unlock();
    return;
  }

  buffer[write_cursor++] = c;

  if (write_cursor == buffer.size())
    write_cursor = 0;

  buf_length++;

  if (c == termios.c_cc[VEOL] && is_lflag_set(ICANON)) {
    read_event.trigger();
  }

  else if (!is_lflag_set(ICANON)) {
    read_event.trigger();
  }

  lock.unlock();
}

char TTY::pop() {
  if (buf_length == 0)
    return 0;

  char c = buffer[read_cursor++];

  if (read_cursor == buffer.size())
    read_cursor = 0;

  buf_length--;
  return c;
}

char TTY::erase() {
  if (buf_length == 0)
    return -1;

  size_t prev_index = 0;

  if (write_cursor == 0) {
    prev_index = buffer.size() - 1;
  } else {
    prev_index = write_cursor - 1;
  }

  auto c = buffer[prev_index];

  if (c == termios.c_cc[VEOL])
    return 0;

  buf_length--;
  write_cursor = prev_index;
  return c;
}

void TTY::input(char c) {
  auto is_canon = is_lflag_set(ICANON);

  // Convert NL to CR
  if (c == '\n' && is_iflag_set(INLCR)) {
    c = '\r';
  } else if (c == '\r') {
    // Ignore CR
    if (is_iflag_set(IGNCR))
      return;

    // Convert CR to NL
    if (is_iflag_set(ICRNL)) {
      c = '\n';
    }
  }

  if (is_canon && c == termios.c_cc[VERASE]) {
    if (erase() >= 0) {
      callback('\b', callback_context);
      callback(' ', callback_context);
      callback('\b', callback_context);
    }

    return;
  }

  if (is_lflag_set(ECHO) || ((c == '\n' && is_lflag_set(ECHONL)))) {
    callback(c, callback_context);
  }

  push(c);
}

Result<size_t, Error> TTY::Ops::write(dev_t minor, frg::span<uint8_t> buf,
                                      off_t off) {

  (void)off;

  auto tty = ttys[minor];

  for (auto c : buf) {
    tty->callback((char)c, tty->callback_context);
  }

  return Ok(buf.size());
}

Result<size_t, Error> TTY::Ops::read(dev_t minor, frg::span<uint8_t> buf,
                                     off_t off) {

  (void)minor;
  (void)off;

  auto nbyte = buf.size();

  auto tty = ttys[minor];

  Vm::Vector<Event *> events;
  events.push(&tty->read_event);

  while (tty->buf_length == 0) {
    await(events);
  }

  if (nbyte > tty->buf_length) {
    nbyte = tty->buf_length;
  }

  size_t i;

  tty->lock.lock();

  for (i = 0; i < nbyte; i++) {
    if (tty->buf_length == 0) {
      break;
    }

    char c = tty->pop();
    buf.data()[i] = c;

    if (tty->termios.c_lflag & ICANON && c == tty->termios.c_cc[VEOL]) {
      break;
    }
  }

  tty->lock.unlock();

  return Ok(i + 1);
}

Result<Fs::VnodeAttr, Error> TTY::Ops::getattr(dev_t minor) {

  (void)minor;

  auto ret = Fs::VnodeAttr{};

  ret.mode = S_IFCHR;

  return Ok(ret);
};

Result<uint64_t, Error> TTY::Ops::ioctl(dev_t minor, uint64_t request,
                                        void *arg) {

  auto tty = ttys[minor];

  struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel; /* unused */
    unsigned short ws_ypixel; /* unused */
  };

  switch (request) {
  case TIOCGWINSZ: {
    auto winsz = (winsize *)arg;
    winsz->ws_col = tty->cols;
    winsz->ws_row = tty->rows;
    return Ok((uint64_t)0);
  }

  case TCSETSF:
  case TCSETSW:
  case TCSETS: {
    tty->termios = *(struct termios *)arg;
    return Ok((uint64_t)0);
  }

  case TCGETS: {
    *(struct termios *)arg = tty->termios;
    return Ok((uint64_t)0);
  }

  default:
    break;
  }

  error("TTY: unimplemented ioctl {}", request);
  return Err(Error::NOT_IMPLEMENTED);
}

} // namespace Gaia::Posix