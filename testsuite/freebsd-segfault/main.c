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
/// Reads four bytes from the symbolic input file and writes to an unmapped
/// address when the first byte is 'A'. Checks that the monitor reports the
/// segfault of the guest process and that a crash test case is generated.
///

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    char buf[4] = {0};

    if (argc < 2) {
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        return 2;
    }
    if (fread(buf, 1, sizeof(buf), f) != sizeof(buf)) {
        return 3;
    }
    fclose(f);

    if (buf[0] == 'A') {
        volatile char *p = (volatile char *) 0x10;
        *p = buf[1]; // page fault on this path
    }

    printf("no crash\n");
    return 0;
}
