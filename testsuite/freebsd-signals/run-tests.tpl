#!/bin/bash

{% include 'common-run.sh.tpl' %}

s2e run -n {{ project_name }}

echo === Checking that the four hardware traps were reported
[ "$(grep -c 'FreeBSDMonitor: Received segfault .* addr=0x10' $S2E_LAST/debug.txt)" -eq 1 ]
[ "$(grep -c 'FreeBSDMonitor: Received trap .* trapnr=1 signr=4' $S2E_LAST/debug.txt)" -eq 1 ]
[ "$(grep -c 'FreeBSDMonitor: Received trap .* trapnr=18 signr=8' $S2E_LAST/debug.txt)" -eq 1 ]
[ "$(grep -c 'FreeBSDMonitor: Received trap .* trapnr=3 signr=5' $S2E_LAST/debug.txt)" -eq 1 ]
[ "$(grep -c 'Terminating state: Segfault' $S2E_LAST/debug.txt)" -eq 1 ]
[ "$(grep -c 'Terminating state: Trap' $S2E_LAST/debug.txt)" -eq 3 ]

echo === Checking that the six signals sent through system calls only produced PROCESS_EXIT
for code in 134 139 132 138 136 133; do
    grep -q "FreeBSDMonitor: Process exit .* exitCode=$code" $S2E_LAST/debug.txt
done
[ "$(grep -c 'FreeBSDMonitor: Received segfault' $S2E_LAST/debug.txt)" -eq 1 ]
[ "$(grep -c 'FreeBSDMonitor: Received trap' $S2E_LAST/debug.txt)" -eq 3 ]
