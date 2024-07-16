/* SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include "lib/spinlock.hpp"
#include <dev/devkit/service.hpp>
#include <frg/list.hpp>
#include <frg/unique.hpp>
#include <frg/vector.hpp>
#include <lib/dot.hpp>
#include <lib/error.hpp>
#include <lib/result.hpp>
#include <vm/heap.hpp>

namespace Gaia::Dev {

class Registry {
public:
  Registry() : root(frg::make_unique<Node>(Vm::get_allocator())) {}
  void set_root(Service *service);
  void add(Service *client, Service *provider);
  frg::string<Gaia::Vm::HeapAllocator> make_graph();

private:
  struct Node {
    Service *provider;
    frg::vector<Service *, Gaia::Vm::HeapAllocator> clients;
  };

  using NodePtr = Vm::UniquePtr<Node>;

  NodePtr root;

  void add_to_graph(DotGraph<Gaia::Vm::HeapAllocator> &graph, Service *parent,
                    Service *child);

  Spinlock spinlock;
};

struct Driver {
  Vm::UniquePtr<Service> (*init)();
};

class Catalog {
public:
  void register_driver(Driver driver, Properties &props);
  Result<Vm::UniquePtr<Service>, Error> find_driver(Service *provider);

private:
  struct CatalogEntry {
    Properties &props;
    Driver driver;
  };

  frg::vector<CatalogEntry, Gaia::Vm::HeapAllocator> drivers;
  Spinlock spinlock;
};

Registry &get_registry();
Catalog &get_catalog();

void create_registry();

} // namespace Gaia::Dev