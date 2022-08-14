# Copyright (c) 2022, lg
# 
# SPDX-License-Identifier: BSD-2-Clause

-include .config

DIRECTORY_GUARD = mkdir -p $(@D)
BASE_CFLAGS = -Isrc/gaia/arch/$(ARCH) -Isrc -Ithirdparty/limine -MMD
ISO = nyx.iso
BUILDDIR = build
KERNEL = $(BUILDDIR)/kernel.elf

ifeq ($(CONFIG_LLVM), y)
	TOOLCHAIN = llvm
endif

ifeq ($(CONFIG_X86_64), y)
	ARCH = x86_64
endif

ifeq ($(CONFIG_DEBUG), y)
	BASE_CFLAGS += -O0 -ggdb
endif

ifeq ($(CONFIG_RELEASE), y)
	BASE_CFLAGS += -O2
endif

-include make/$(ARCH)-$(TOOLCHAIN).mk