#!/bin/sh
# Runs the 32-bit tests of gu_extras.c on Linux x86-64 (no libc needed).
# The tests build a fake game image with the real byte patterns, install the
# hooks over it and execute them.
cd "$(dirname "$0")"
gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -fno-builtin \
    -fno-tree-loop-distribute-patterns -fomit-frame-pointer -msse2 -mfpmath=sse \
    -O1 -std=gnu99 -I.. -nostdlib -static -Wl,-m,elf_i386 \
    test_gu_extras.c -o test_gu_extras && ./test_gu_extras
