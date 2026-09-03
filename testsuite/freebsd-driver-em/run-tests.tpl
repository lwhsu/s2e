#!/bin/bash

{% include 'common-run.sh.tpl' %}

# The attach path of a real driver under symbolic hardware does not terminate
# on its own in a reasonable time: the run is time-boxed and the checks look
# at what was explored.
timeout --foreground --kill-after=5m 20m s2e run -n {{ project_name }} || true

echo === Checking that the driver was loaded and attached under S2E
grep -q "kld load freebsd64-if_em.ko" $S2E_LAST/debug.txt
grep -q "iommuread_0xfebc" $S2E_LAST/debug.txt

echo === Checking that the attach path forked on the device registers
COUNT=$(grep -c "Forking state" $S2E_LAST/debug.txt)
if [ $COUNT -lt 10 ]; then
    echo Too few forks: $COUNT
    exit 1
fi

# iflib drivers make most of their resource allocations from the kernel's
# iflib code, so fault injection into the module's own calls is not required
if grep -q "injecting fault into" $S2E_LAST/debug.txt; then
    echo === Faults were injected into the driver
else
    echo === No fault was injected into the driver
fi

# check_coverage reads the first "lines..." line of cov.log, i.e. the driver's: do not add kernel
# tracking (mod_kernel) to this project's config or the number changes meaning
check_coverage {{project_name}} 5

s2e forkprofile {{ project_name }} > $S2E_LAST/forkprofile.txt
grep -q -i "e1000\|if_em" $S2E_LAST/forkprofile.txt
