add_languages('nasm')

kernel_srcs += files(
        'apic.cpp',
        'entry.cpp',
        'gdt.cpp',
        'hal.cpp',
        'hpet.cpp',
        'idt.cpp',
        'int.cpp',
        'locore.asm',
        'mmu.cpp',
        'simd.cpp',
        'smp.cpp',
        'syscall.asm',
        'syscall.cpp',
        'timer.cpp',
)

cpp_args += [
        '-mabi=sysv',
        '-mno-80387',
        '-mno-mmx',
        '-mno-3dnow',
        '-mno-sse',
        '-mno-sse2',
        '-mno-ssse3',
        '-mno-sse4',
        '-mno-sse4a',
        '-mno-sse4.1',
        '-mno-sse4.2',
        '-mno-avx',
        '-mno-avx2',
        '-mno-avx512f',
        '-mno-red-zone',
        '-mcmodel=kernel',
]