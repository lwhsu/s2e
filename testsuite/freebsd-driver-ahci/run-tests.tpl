#!/bin/bash

{% include 'common-run.sh.tpl' %}

# The attach path of a real driver under symbolic hardware does not terminate
# on its own in a reasonable time: the run is time-boxed and the checks look
# at what was explored.
# Only the timeout (exit 124) is tolerated; any other failure of s2e run is a test failure
timeout --foreground --kill-after=5m 20m s2e run -n {{ project_name }} || [ $? -eq 124 ]

echo === Checking that the driver was loaded and attached under S2E
grep -q "kld load freebsd64-ahci.ko" $S2E_LAST/debug.txt
grep -q "iommuread_0xfebf1" $S2E_LAST/debug.txt

echo === Checking that the attach path forked on the device registers
COUNT=$(grep -c "Forking state" $S2E_LAST/debug.txt)
if [ $COUNT -lt 10 ]; then
    echo Too few forks: $COUNT
    exit 1
fi

echo === Checking that the bus_dma hooks were reached by the driver
# ahci allocates the command lists and FIS area of every channel with bus_dmamem_alloc()/bus_dmamap_create() (inline
# wrappers around the bus_dma_impl methods) during attach
grep -q "FaultInjInvokeOrig_bus_dmamem_alloc\|FaultInjInvokeOrig_bus_dmamap_create" $S2E_LAST/debug.txt
grep -q "injecting fault into bus_dma" $S2E_LAST/debug.txt

# check_coverage reads the first "lines..." line of cov.log, i.e. the driver's
check_coverage {{project_name}} 5

s2e forkprofile {{ project_name }} > $S2E_LAST/forkprofile.txt
grep -q -i "ahci" $S2E_LAST/forkprofile.txt
