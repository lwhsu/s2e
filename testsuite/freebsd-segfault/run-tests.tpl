#!/bin/bash

{% include 'common-run.sh.tpl' %}

s2e run -n {{ project_name }}

echo === Checking that both paths were explored
grep -q "no crash" $S2E_LAST/debug.txt
grep -q "FreeBSDMonitor: Received segfault .* addr=0x10" $S2E_LAST/debug.txt
grep -q "Terminating state: Segfault" $S2E_LAST/debug.txt

echo === Checking that a crash test case was generated
ls $S2E_LAST/testcase-crash* > /dev/null
