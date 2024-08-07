/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include "lib/stream.hpp"
#include <frg/span.hpp>
#include <lib/base.hpp>
#include <lib/error.hpp>
#include <lib/path.hpp>
#include <lib/result.hpp>
#include <sys/types.h>
#include <vm/heap.hpp>

#define __USE_MISC
#include <sys/stat.h>

namespace Gaia::Fs {

struct Vnode;

struct VnodeAttr {
  size_t size;
  mode_t mode;
  uid_t uid;
  gid_t gid;
  time_t time;
};

constexpr VnodeAttr DefaultVnodeAttr = {
    .size = 0,
    .mode = 0,
    .uid = 0,
    .gid = 0,
    .time = 0,
};

struct VnodeOps {
  virtual Result<Vnode *, Error> lookup(Vnode *dir, frg::string_view path) = 0;

  // FIXME (hack): Use type or something instead of checking whether dev_t != -1
  virtual Result<Vnode *, Error> create(Vnode *dir, frg::string_view path,
                                        VnodeAttr attr = DefaultVnodeAttr,
                                        dev_t dev = -1) = 0;
  virtual Result<Vnode *, Error> mkdir(Vnode *dir, frg::string_view path,
                                       VnodeAttr attr = DefaultVnodeAttr) = 0;

  virtual Result<Vnode *, Error> link(Vnode *dir, frg::string_view path,
                                      frg::string_view link,
                                      VnodeAttr attr = DefaultVnodeAttr) = 0;

  virtual Result<VnodeAttr, Error> getattr(Vnode *node) = 0;
  virtual Result<size_t, Error> read(Vnode *vn, frg::span<uint8_t> buf,
                                     off_t off) = 0;
  virtual Result<size_t, Error> write(Vnode *vn, frg::span<uint8_t> buf,
                                      off_t off) = 0;

  virtual Result<uint64_t, Error> ioctl(Vnode *vn, uint64_t request,
                                        void *arg) = 0;

  virtual Vm::String get_absolute_path(Vnode *vn) = 0;

  virtual Result<size_t, Error> readdir(Vnode *vn, void *buf, size_t max_size,
                                        off_t offset) = 0;
};

struct Vnode {
  enum class Type {
    NONE,
    REG,
    DIR,
    CHR,
    LNK,
  };

  Type type;
  void *data;

  dev_t dev;
  VnodeOps *ops;

  Vnode(Type type, void *data, VnodeOps *ops)
      : type(type), data(data), ops(ops) {}
};

Result<size_t, Error> vfs_read(Vnode *vn, frg::span<uint8_t> buf, off_t off);
Result<size_t, Error> vfs_readdir(Vnode *vn, void *buf, size_t max_size,
                                  off_t off);
Result<size_t, Error> vfs_write(Vnode *vn, frg::span<uint8_t> buf, off_t off);

Result<Vnode *, Error> vfs_create_file(Vnode *cwd, frg::string_view name,
                                       VnodeAttr attr = DefaultVnodeAttr,
                                       dev_t dev = -1);
Result<Vnode *, Error> vfs_mkdir(Vnode *cwd, frg::string_view name,
                                 VnodeAttr attr = DefaultVnodeAttr);

Result<Vnode *, Error> vfs_find(frg::string_view pathname,
                                Vnode *cwd = nullptr);

Result<Vnode *, Error> vfs_link(Vnode *cwd, frg::string_view path,
                                frg::string_view link,
                                VnodeAttr attr = DefaultVnodeAttr);

Vm::String vfs_get_absolute_path(Vnode *node);
extern Vnode *root_vnode;

template <typename F>
Result<Vnode *, Error> vfs_find_and(frg::string_view pathname, F function,
                                    Vnode *cwd = nullptr,
                                    VnodeAttr attr = DefaultVnodeAttr) {
  Path<Gaia::Vm::HeapAllocator> path{pathname};

  auto segments = path.parse();

  Vnode *vn = nullptr;
  bool root = false;

  if (segments[0][0] == '/') {
    vn = root_vnode;
    root = true;
    log("Path is root, {:x}", vn);
  } else {
    vn = cwd ? cwd : root_vnode;
    log("cwd is {}", cwd);
  }

  for (size_t i = root; i < segments.size(); i++) {
    if (i != segments.size() - 1) {
      vn = TRY(vn->ops->lookup(vn, segments[i]));
    } else {
      vn = TRY(function(vn, segments[i], attr));
    }
  }

  return Ok(vn);
}

template <typename F>
Result<Vnode *, Error> vfs_find_and(frg::string_view pathname, dev_t dev,
                                    F function, Vnode *cwd = nullptr,
                                    VnodeAttr attr = DefaultVnodeAttr) {
  Path<Gaia::Vm::HeapAllocator> path{pathname};
  auto segments = path.parse();

  Vnode *vn;
  bool root = false;

  if (segments[0][0] == '/') {
    vn = root_vnode;
    root = true;
  } else {
    vn = cwd ? cwd : root_vnode;
  }

  for (size_t i = root; i < segments.size(); i++) {
    if (i != segments.size() - 1) {
      vn = TRY(vn->ops->lookup(vn, segments[i]));
    } else {
      vn = TRY(function(vn, segments[i], attr, dev));
    }
  }

  return Ok(vn);
}

template <typename F>
Result<Vnode *, Error>
vfs_find_and(frg::string_view pathname, frg::string_view linkpath, F function,
             Vnode *cwd = nullptr, VnodeAttr attr = DefaultVnodeAttr) {
  Path<Gaia::Vm::HeapAllocator> path{pathname};
  auto segments = path.parse();

  Vnode *vn;
  bool root = false;

  if (segments[0][0] == '/') {
    vn = root_vnode;
    root = true;
  } else {
    vn = cwd ? cwd : root_vnode;
  }

  for (size_t i = root; i < segments.size(); i++) {
    if (i != segments.size() - 1) {
      vn = TRY(vn->ops->lookup(vn, segments[i]));
    } else {
      vn = TRY(function(vn, segments[i], linkpath, attr));
    }
  }

  return Ok(vn);
}

class VnodeStream : public Stream {

public:
  VnodeStream(Vnode *vnode) : position(0), vnode(vnode) {}

  Result<size_t, Error> read(void *buf, size_t size) override {
    auto ret = TRY(vfs_read(vnode, {(uint8_t *)buf, size}, position));
    position += ret;
    return Ok(ret);
  }

  Result<size_t, Error> write(void *buf, size_t size) override {
    auto ret = TRY(vfs_write(vnode, {(uint8_t *)buf, size}, position));
    position += ret;
    return Ok(ret);
  }

  Result<off_t, Error> seek(off_t offset, Whence whence) override {
    switch (whence) {
    case Whence::CURRENT:
      position += offset;
      break;
    case Whence::SET:
      position = offset;
      break;
    case Whence::END:
      position = TRY(vnode->ops->getattr(vnode)).size + offset;
      break;
    default:
      return Err(Error::INVALID_PARAMETERS);
    }

    return Ok(position);
  }

private:
  off_t position;
  Vnode *vnode;
};

} // namespace Gaia::Fs
