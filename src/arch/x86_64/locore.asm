; SPDX-License-Idenitifer: BSD-2-Clause

bits 64

global gdt_load

gdt_load:
  lgdt [rdi]

  ; Set ss,ds and es to data segment
  mov ax, 0x30
  mov ss, ax
  mov ds, ax
  mov es, ax
  pop rdi

  ; Push the code segment so that the cpu will pop it off through retfq
  mov rax, 0x28
  push rax
  push rdi
  retfq


global tss_reload
tss_reload:
  mov ax, 0x48
  ltr ax
  ret


%macro SWAPGS_IF_NEEDED 0
     cmp qword [rsp+16], 0x43
     jne %%1
     swapgs
%%1:
%endmacro

%macro INTERRUPT_NAME 1
dq __interrupt%1
%endmacro

%macro INTERRUPT_ERR 1
__interrupt%1:
    push %1
    SWAPGS_IF_NEEDED
    jmp __interrupt_common
%endmacro

%macro INTERRUPT_NOERR 1
__interrupt%1:
    push 0    ; no error
    push %1
    SWAPGS_IF_NEEDED
    jmp __interrupt_common
%endmacro

%macro pushaq 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popaq 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro



extern interrupts_handler

__interrupt_common:
    cld

    pushaq

    mov rdi, rsp

    call interrupts_handler

    mov rsp, rax

    popaq

    cli
    SWAPGS_IF_NEEDED

    add rsp, 16 ; pop errcode and int number

    iretq

extern intr_timer_handler

global timer_interrupt

timer_interrupt:
    push 0
    push 32
    SWAPGS_IF_NEEDED
    jmp __timer_interrupt

__timer_interrupt:
    cld

    pushaq

    mov rdi, rsp

    call intr_timer_handler
    mov rsp, rax
    popaq

    cli
    SWAPGS_IF_NEEDED

    add rsp, 16 ; pop errcode and int number

    iretq



INTERRUPT_NOERR 0
INTERRUPT_NOERR 1
INTERRUPT_NOERR 2
INTERRUPT_NOERR 3
INTERRUPT_NOERR 4
INTERRUPT_NOERR 5
INTERRUPT_NOERR 6
INTERRUPT_NOERR 7
INTERRUPT_ERR   8
INTERRUPT_NOERR 9
INTERRUPT_ERR   10
INTERRUPT_ERR   11
INTERRUPT_ERR   12
INTERRUPT_ERR   13
INTERRUPT_ERR   14
INTERRUPT_NOERR 15
INTERRUPT_NOERR 16
INTERRUPT_ERR   17
INTERRUPT_NOERR 18
INTERRUPT_NOERR 19
INTERRUPT_NOERR 20
INTERRUPT_NOERR 21
INTERRUPT_NOERR 22
INTERRUPT_NOERR 23
INTERRUPT_NOERR 24
INTERRUPT_NOERR 25
INTERRUPT_NOERR 26
INTERRUPT_NOERR 27
INTERRUPT_NOERR 28
INTERRUPT_NOERR 29
INTERRUPT_ERR   30
INTERRUPT_NOERR 31

%assign i 32
%rep 224
    INTERRUPT_NOERR i
%assign i i+1
%endrep


section .data
global __interrupt_vector

__interrupt_vector:
%assign i 0
%rep 256
    INTERRUPT_NAME i
%assign i i+1
%endrep
