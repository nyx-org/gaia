/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef SRC_KERN_SYSCALL_H
#define SRC_KERN_SYSCALL_H
#include <libkern/base.h>

typedef struct {
    uintptr_t num;
    uintptr_t param1;
    uintptr_t param2;
    uintptr_t param3;
    uintptr_t param4;
    uintptr_t param5;
    intr_frame_t *frame;
} syscall_frame_t;

enum syscalls {
    SYS_DEBUG,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_READ,
    SYS_WRITE,
    SYS_STAT,
    SYS_SEEK,
    SYS_MMAP,
    SYS_EXIT,
    SYS_TCB_SET,
    SYS_GETPID,
    SYS_GETPPID,
    SYS_FORK,
    SYS_EXEC,
    SYS_READDIR,
    SYS_WAITPID,
    SYS_IOCTL,
};

uintptr_t syscall_handler(syscall_frame_t frame);

#endif
