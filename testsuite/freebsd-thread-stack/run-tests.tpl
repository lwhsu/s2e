#!/bin/bash

{% include 'common-run.sh.tpl' %}

s2e run -n {{ project_name }}

grep -q "icount good" $S2E_LAST/debug.txt

echo === Checking that every thread got a stack
if grep -q "could not get current stack" $S2E_LAST/debug.txt; then
    echo "StackMonitor could not get the stack of some thread"
    exit 1
fi

# The main thread and 16 threads
TIDS=$(grep -o "update pid=0x[0-9a-f]* tid=0x[0-9a-f]*" $S2E_LAST/debug.txt | sort -u | wc -l)
if [ $TIDS -lt 17 ]; then
    echo "Expected updates from at least 17 threads, got $TIDS"
    exit 1
fi

# Each thread stack is a different mapping, so a different bound
BOUNDS=$(grep -o "Stack bound=0x[0-9a-f]*" $S2E_LAST/debug.txt | sort -u | wc -l)
if [ $BOUNDS -lt 17 ]; then
    echo "Expected at least 17 different stack bounds, got $BOUNDS"
    exit 1
fi
