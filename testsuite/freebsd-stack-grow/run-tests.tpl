#!/bin/bash

{% include 'common-run.sh.tpl' %}

s2e run -n {{ project_name }}

grep -q "stackgrow done" $S2E_LAST/debug.txt
grep -q "stackgrow: tid=.* deep sp" $S2E_LAST/debug.txt
grep -q "stackgrow: tid=.* top sp" $S2E_LAST/debug.txt

echo === Checking that the thread stack bound covers the whole chain
if grep -q "goes above stack bound" $S2E_LAST/debug.txt; then
    echo "StackMonitor saw the stack pointer above the bound: the chain was not merged"
    exit 1
fi
if grep -q "could not get current stack" $S2E_LAST/debug.txt; then
    exit 1
fi
# The main thread and the worker thread have different bounds
BOUNDS=$(grep -o "Stack bound=0x[0-9a-f]*" $S2E_LAST/debug.txt | sort -u | wc -l)
[ "$BOUNDS" -ge 2 ]
