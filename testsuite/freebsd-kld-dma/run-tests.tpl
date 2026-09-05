#!/bin/bash

{% include 'common-run.sh.tpl' %}

s2e run -n {{ project_name }}

echo === Checking that the three bus_dma calls were each injected once
grep -q "injecting fault into bus_dma_tag_create" $S2E_LAST/debug.txt
grep -q "injecting fault into bus_dmamem_alloc" $S2E_LAST/debug.txt
grep -q "injecting fault into bus_dmamap_create" $S2E_LAST/debug.txt
grep -q "dmatest: bus_dma_tag_create failed" $S2E_LAST/debug.txt
grep -q "dmatest: bus_dmamem_alloc failed 12, taking the error path" $S2E_LAST/debug.txt
grep -q "dmatest: bus_dmamap_create failed 12, taking the error path" $S2E_LAST/debug.txt
grep -q "dmatest: bus_dmamem_alloc ok" $S2E_LAST/debug.txt
grep -q "dmatest: bus_dmamap_create ok" $S2E_LAST/debug.txt

COUNT=$(grep -c "dmatest: done" $S2E_LAST/debug.txt)
if [ $COUNT -ne 4 ]; then
    echo Incorrect number of completed paths: $COUNT
    exit 1
fi
