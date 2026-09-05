#!/bin/sh

# Copyright (c) 2026 Li-Wen Hsu
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
set -e

echo "Patching bootstrap.sh..."

# The AHCI controller (pci0:0:4:0 in the s2ekernel-ahci image, after the e1000 at 3.0)
# is disabled before the driver is loaded, so that the attach runs after the kernel
# announced the module. The module in the image is ahci.ko; the analyzed copy is
# loaded under its test name.
sed -i.bak 's|^    kldload "${TARGET}"$|    devctl disable pci0:0:4:0\n    kldload "./${TARGET}"\n    devctl enable pci0:0:4:0\n    sleep 10|' "$PROJECT_DIR/bootstrap.sh" \
    && rm -f "$PROJECT_DIR/bootstrap.sh.bak"
grep -q "devctl enable" "$PROJECT_DIR/bootstrap.sh"

echo "Patching s2e-config.lua..."

cat << LUA >> $PROJECT_DIR/s2e-config.lua

-- The AHCI controller of the s2ekernel-ahci image: ABAR (bar[24]) 0xfebf1000-0xfebf1fff, legacy I/O
-- bar[20] 0xc040-0xc05f. The registers are kept concrete on purpose: with symbolic MMIO the port
-- polling loops of the attach path fork faster than the searcher reaches the fault injection
-- branches (46 forks and no injection in 20 minutes), and the point of this test is that the
-- bus_dma hooks fire on a real driver. Uncomment to add symbolic hardware on top.
-- pluginsConfig.SymbolicHardware = {
--     ahci = { ports = { {0xc040, 0xc05f} }, mmio = { {0xfebf1000, 0xfebf1fff} } },
-- }

-- Bound the exploration of the attach path (register polling loops fork at the same pc forever)
pluginsConfig.ForkLimiter.maxForkCount = 5
LUA
