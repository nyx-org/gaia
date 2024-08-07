/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include <frg/array.hpp>
#include <fs/vfs.hpp>
#include <lib/charon.hpp>
#include <lib/list.hpp>

namespace Gaia::Fs {

struct TmpNode;

struct TmpDirent {
  char name[256];

  ListNode<TmpDirent> link;

  TmpNode *tnode;

  TmpDirent(){};
};

struct TmpNode {
  Vnode *vnode = nullptr;
  VnodeAttr attr;
  Vnode::Type type;

  TmpNode *parent;

  union {
    /* VDIR */
    struct {
      List<TmpDirent, &TmpDirent::link> entries;
    } dir;

    /* VREG */
    struct {
      void *buffer;
    } reg;

    /* VLNK */
    struct {
      frg::string_view to_name;
      TmpNode *to;
    } link;
  };

  TmpNode(){};
  TmpNode(TmpNode *dir, Vnode::Type type, frg::string_view name,
          VnodeAttr attr = DefaultVnodeAttr,
          frg::optional<frg::string_view> symlink_path = frg::null_opt);

  Vnode *make_vnode(dev_t = -1);
};

void tmpfs_init(Charon charon);

} // namespace Gaia::Fs
