#!/bin/bash

{% include 'common-run.sh.tpl' %}

timeout --foreground --kill-after=10m 40m s2e run -n {{ project_name }}

echo === Checking that the driver was loaded and attached under S2E
grep -q "kld load freebsd64-if_em.ko" $S2E_LAST/debug.txt
grep -q "iommuread_0xfebc" $S2E_LAST/debug.txt

echo === Checking that the attach path forked on the device registers
COUNT=$(grep -c "Forking state" $S2E_LAST/debug.txt)
if [ $COUNT -lt 10 ]; then
    echo Too few forks: $COUNT
    exit 1
fi

echo === Checking that faults were injected into the driver
grep -q "injecting fault into" $S2E_LAST/debug.txt

check_coverage {{project_name}} 5

s2e forkprofile {{ project_name }} > $S2E_LAST/forkprofile.txt
grep -q -i "e1000\|if_em" $S2E_LAST/forkprofile.txt
