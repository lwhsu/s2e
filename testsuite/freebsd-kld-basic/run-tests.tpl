#!/bin/bash

{% include 'common-run.sh.tpl' %}

s2e run -n {{ project_name }}

echo === Checking that the module ran on every path
grep -q "s2etest: port value is 0x42" $S2E_LAST/debug.txt
grep -q "s2etest: port value is not 0x42" $S2E_LAST/debug.txt
grep -q "s2etest: mmio bit 0 is set" $S2E_LAST/debug.txt
grep -q "s2etest: mmio bit 0 is clear" $S2E_LAST/debug.txt
grep -q "s2etest: malloc failed, taking the error path" $S2E_LAST/debug.txt
grep -q "s2etest: malloc succeeded" $S2E_LAST/debug.txt
grep -q "injecting fault into malloc" $S2E_LAST/debug.txt

COUNT=$(grep -c "s2etest: done" $S2E_LAST/debug.txt)
if [ $COUNT -ne 8 ]; then
    echo Incorrect number of completed paths: $COUNT
    exit 1
fi

check_coverage {{project_name}} 90

s2e forkprofile {{ project_name }} > $S2E_LAST/forkprofile.txt
grep -q -i s2etest.c $S2E_LAST/forkprofile.txt
