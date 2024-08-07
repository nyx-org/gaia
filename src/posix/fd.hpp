/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include "lib/stream.hpp"
#include <frg/array.hpp>
#include <frg/bitset.hpp>
#include <frg/optional.hpp>
#include <fs/vfs.hpp>
#include <lib/base.hpp>

#define HAVE_ARCH_STRUCT_FLOCK
#include <dirent.h>
#include <linux/fcntl.h>
#include <posix/errno.hpp>
#include <sys/stat.h>
#include <sys/types.h>

#define OFLAG_WRITABLE(flag) (((flag) & O_RDWR) || ((flag) & O_WRONLY))

namespace Gaia::Posix {

using namespace Fs;

constexpr auto OPEN_MAX = 64;

class Fd {
public:
  static Result<Fd, Errno> open(const char *path, int status) {
    auto res = vfs_find(path);

    // O_CREAT and O_EXCL are set, and the named file exists.
    if (res.is_ok() && (status & O_CREAT) && (status & O_EXCL)) {
      return Err(EEXIST);
    }

    log("res: {}", res.is_ok());

    if (res.is_ok()) {
      auto vnode = res.value().value();

      // The named file is a directory and oflag includes O_WRONLY or O_RDWR.
      if (vnode->type == Vnode::Type::DIR && OFLAG_WRITABLE(status)) {
        return Err(EISDIR);
      }

      if (status & O_DIRECTORY && vnode->type != Vnode::Type::DIR) {
        return Err(ENOTDIR);
      }
    }

    if (res.is_err() && status & O_CREAT) {
      res = vfs_find_and(path, -1, vfs_create_file);
    }

    if (res.is_err()) {
      return Err(error_to_errno(res.error().value()));
    }

    auto vnode = res.value().value();

    return Ok(Fd{vnode, status});
  }

  static Result<Fd, Errno> openat(Fd dirfd, const char *path, int status) {
    auto res = vfs_find(path, dirfd.vnode);

    // O_CREAT and O_EXCL are set, and the named file exists.
    if (res.is_ok() && (status & O_CREAT) && (status & O_EXCL)) {
      return Err(EEXIST);
    }

    if (res.is_ok()) {
      auto vnode = res.value().value();

      // The named file is a directory and oflag includes O_WRONLY or O_RDWR.
      if (vnode->type == Vnode::Type::DIR && OFLAG_WRITABLE(status)) {
        return Err(EISDIR);
      }
    }

    if (res.is_err() && status & O_CREAT) {
      res = vfs_find_and(path, -1, vfs_create_file, dirfd.vnode);
    }

    if (res.is_err()) {
      return Err(error_to_errno(res.error().value()));
    }

    auto vnode = res.value().value();

    return Ok(Fd{vnode, status});
  }

  Result<off_t, Errno> seek(off_t offset, Stream::Whence whence) {
    auto res = stream.seek(offset, whence);

    if (res.is_err()) {
      return Err(error_to_errno(res.error().value()));
    }

    return Ok(res.value().value());
  }

  Result<size_t, Errno> write(void *buf, size_t count) {
    if (!(status & O_RDWR) && !(status & O_WRONLY)) {
      return Err(EBADF);
    }

    auto res = stream.write(buf, count);

    if (res.is_err()) {
      return Err(error_to_errno(res.error().value()));
    }

    return Ok(res.value().value());
  }

  Result<size_t, Errno> read(void *buf, size_t count) {

    // O_RDONLY is 0 so that we need to check flags != O_RDONLY
    if (!(status & O_RDWR) && !(status & O_RDONLY) && status != O_RDONLY) {
      return Err(EBADF);
    }

    auto res = stream.read(buf, count);

    if (res.is_err()) {
      return Err(error_to_errno(res.error().value()));
    }

    return Ok(res.value().value());
  }

  Result<uint64_t, Errno> ioctl(uint64_t request, void *arg) {
    auto res = vnode->ops->ioctl(vnode, request, arg);

    if (res.is_err()) {
      return Err(error_to_errno(res.error().value()));
    }

    return Ok(res.value().value());
  }

  Result<struct stat, Errno> stat() {

    struct stat ret {};

    auto attr = DefaultVnodeAttr;

    auto res = vnode->ops->getattr(vnode);

    if (res.is_err()) {
      return Err(error_to_errno(res.error().value()));
    }

    attr = res.unwrap();

    ret.st_mode = attr.mode;
    ret.st_blksize = 512;
    ret.st_size = attr.size;
    ret.st_dev = 0;
    ret.st_ino = (ino_t)vnode->data;
    ret.st_rdev = vnode->dev;
    ret.st_blocks = attr.size / 512;

    switch (vnode->type) {
    case Vnode::Type::REG:
      ret.st_mode |= S_IFREG;
      break;
    case Vnode::Type::DIR:
      ret.st_mode |= S_IFDIR;
      break;
    case Vnode::Type::LNK:
      ret.st_mode |= S_IFLNK;
      break;
    case Vnode::Type::CHR:
      ret.st_mode |= S_IFCHR;
      break;
    default:
      break;
    }
    return Ok(ret);
  }

  Result<size_t, Errno> readdir(void *buf, size_t max_size) {
    auto position = stream.seek(0, Stream::Whence::CURRENT).unwrap();

    auto res = vfs_readdir(vnode, buf, max_size, position);

    if (res.is_err()) {
      return Err(error_to_errno(res.error().value()));
    }

    auto ret = res.value().value();

    stream.seek(ret, Stream::Whence::CURRENT);

    return Ok(ret);
  }

  int get_flags() { return flags; }

  void set_flags(int val) { flags = val; }

  int get_status() { return status; }

  void set_status(int val) { status = val; }

  void close();

  Fd(Vnode *vn, int status) : vnode(vn), stream(vn), status(status) {}

  Fd() : vnode(nullptr), stream(nullptr), flags(0) {}

  Fd(Fd &&other)
      : vnode(other.vnode), stream(other.stream), flags(other.flags),
        status(other.status){};

  Fd(Fd &other)
      : vnode(other.vnode), stream(other.stream), flags(other.flags),
        status(other.status){};

  Fd(Fd const &other)
      : vnode(other.vnode), stream(other.stream), flags(other.flags),
        status(other.status){};

private:
  Vnode *vnode;
  VnodeStream stream;
  int flags, status;
};

constexpr auto FD_MAX = 256;

class Fds {
public:
  frg::optional<int> allocate(Fd *fd) {
    for (size_t i = 0; i < FD_MAX; i++) {
      if (!bitmap.test(i)) {
        bitmap.set(i);
        fds[i] = fd;
        return i;
      }
    }

    return frg::null_opt;
  }

  frg::optional<int> allocate_greater_than_or_equal(int fdnum, Fd *fd) {
    for (size_t i = fdnum; i < FD_MAX; i++) {
      if (!bitmap.test(i)) {
        bitmap.set(i);
        fds[i] = fd;
        return i;
      }
    }

    return frg::null_opt;
  }

  Result<Void, Errno> free(int fdnum) {
    if (fdnum > FD_MAX || fdnum < 0)
      return Err(EBADF);

    if (!bitmap.test(fdnum))
      return Err(EBADF);

    bitmap.flip(fdnum);

    return Ok({});
  }

  frg::optional<Fd *> get(int number) {
    if (number > FD_MAX || number < 0) {
      return frg::null_opt;
    }

    if (bitmap.test(number)) {
      return fds[number];
    }

    return frg::null_opt;
  }

  Result<Void, Errno> set(int number, Fd *fd) {
    if (number > FD_MAX || number < 0) {
      return Err(EINVAL);
    }

    ASSERT(!bitmap.test(number) && "Handle closing FDs in dup2");

    bitmap.set(number);
    fds[number] = fd;

    return Ok({});
  }

  inline Fds &operator=(const Fds &other) {
    fds = other.fds;
    bitmap = other.bitmap;
    return *this;
  }

  frg::array<Fd *, FD_MAX> data() { return fds; }

  Fds() { bitmap.reset(); }

  Fds(Fds &other) : fds(other.fds), bitmap(other.bitmap) {}

private:
  frg::array<Fd *, FD_MAX> fds;
  frg::bitset<FD_MAX> bitmap;
};

} // namespace Gaia::Posix