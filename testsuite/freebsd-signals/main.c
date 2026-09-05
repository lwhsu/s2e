/// S2E Selective Symbolic Execution Platform
///
/// Copyright (c) 2026 Li-Wen Hsu
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE

///
/// Which fatal signals does s2e.ko report as SEGFAULT/TRAP? Reads one byte from the
/// symbolic input file and takes one path per value. Signals delivered through a
/// system call (abort(), kill(2), raise()) must only produce PROCESS_EXIT; real
/// hardware traps (ud2, divide by zero, page fault, int3) must be reported.
///
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    char c = 0;
    volatile int zero = 0;
    volatile int r;

    if (argc < 2) {
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        return 2;
    }
    if (fread(&c, 1, 1, f) != 1) {
        return 3;
    }
    fclose(f);

    if (c == 'A') {
        abort();
    } else if (c == 'B') {
        kill(getpid(), SIGSEGV);
    } else if (c == 'C') {
        raise(SIGILL);
    } else if (c == 'D') {
        __asm__ volatile("ud2");
    } else if (c == 'E') {
        r = 1 / zero;
    } else if (c == 'F') {
        *(volatile char *) 0x10 = 1;
    } else if (c == 'G') {
        __asm__ volatile("int3");
    } else if (c == 'H') {
        kill(getpid(), SIGBUS);
    } else if (c == 'I') {
        kill(getpid(), SIGFPE);
    } else if (c == 'J') {
        kill(getpid(), SIGTRAP);
    } else {
        printf("ok\n");
    }
    printf("after %c\n", c);
    return 0;
}
