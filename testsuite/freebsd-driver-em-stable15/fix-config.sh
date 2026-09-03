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

# The NIC (pci0:0:3:0 in the S2E images) is disabled before the driver is
# loaded, so that the attach runs after the kernel announced the module.
# The module in the image is if_em.ko; the analyzed copy is loaded under its
# test name, so the stock one must not be found first.
sed -i.bak 's|^    kldload "${TARGET}"$|    devctl disable pci0:0:3:0\n    kldload "./${TARGET}"\n    devctl enable pci0:0:3:0\n    sleep 10|' "$PROJECT_DIR/bootstrap.sh" \
    && rm -f "$PROJECT_DIR/bootstrap.sh.bak"
grep -q "devctl enable" "$PROJECT_DIR/bootstrap.sh"

echo "Patching s2e-config.lua..."

cat << LUA >> $PROJECT_DIR/s2e-config.lua

-- The e1000 NIC of the S2E images: port 0xc000-0xc03f, BAR0 0xfebc0000-0xfebdffff
pluginsConfig.SymbolicHardware = {
    e1000 = {
        ports = {
            {0xc000, 0xc03f},
        },
        mmio = {
            {0xfebc0000, 0xfebdffff},
        },
    },
}

-- Bound the exploration of the attach path (register polling loops fork at the same pc forever)
pluginsConfig.ForkLimiter.maxForkCount = 5
LUA
