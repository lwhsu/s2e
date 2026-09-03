#!/bin/bash

{% include 'common-run.sh.tpl' %}

s2e run -n {{ project_name }}

echo === Checking that the panic reached S2E and the other path completed
grep -q "s2epanic: no panic on this path" $S2E_LAST/debug.txt
grep -q "s2epanic: done" $S2E_LAST/debug.txt
grep -q "Kernel panic: s2epanic: deliberate panic" $S2E_LAST/warnings.txt
grep -q "Terminating state.*Kernel panic: s2epanic: deliberate panic" $S2E_LAST/debug.txt

echo === Checking that the guest did not stop in ddb
if grep -q "^db> \|KDB: enter: panic" serial.txt; then
    echo "The guest entered ddb"
    exit 1
fi
