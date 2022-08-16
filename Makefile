# Copyright (c) 2022, lg
# 
# SPDX-License-Identifier: BSD-2-Clause

-include make/config.mk

KERNEL_C_SOURCES = 	 $(wildcard src/gaia/*.c) \
				 	 $(wildcard src/gaia/*/*.c) \
				 	 $(wildcard src/gaia/arch/$(ARCH)/*.c) \
				 	 $(wildcard src/lib/gaia/*.c) \
				 	 $(wildcard src/lib/stdc-shim/*.c) \
				
KERNEL_ASM_SOURCES = $(wildcard src/gaia/arch/$(ARCH)/*.s)


KERNEL_C_OBJS = $(patsubst %.c, $(BUILDDIR)/%.c.o, $(KERNEL_C_SOURCES))
KERNEL_OBJS = $(KERNEL_C_OBJS) \
			  $(patsubst %.s, $(BUILDDIR)/%.s.o, $(KERNEL_ASM_SOURCES))

all: $(ISO)

$(ISO): $(KERNEL)
	./scripts/make-image.sh $(BUILDDIR)

$(KERNEL): $(KERNEL_OBJS)
	$(LD) -o $@ $^ $(KERNEL_LINK_FLAGS)

$(BUILDDIR)/%.c.o: %.c
	@$(DIRECTORY_GUARD)
	$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(BUILDDIR)/%.s.o: %.s
	@$(DIRECTORY_GUARD)
	$(AS) $(ASFLAGS) $< -o $@

run: $(ISO)
	qemu-system-$(ARCH) -m 4G -cpu host -M q35 -no-reboot -debugcon stdio -enable-kvm -cdrom $^

clean:
	-rm -rf $(BUILDDIR)

.PHONY: all run clean

DEPS += $(KERNEL_C_OBJS:.c.o=.c.d)
-include $(DEPS)