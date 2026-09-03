#!/bin/bash

{% include 'common-run.sh.tpl' %}

timeout --foreground --kill-after=30m 25m s2e run -n {{ project_name }}

grep -q "You lose" $S2E_LAST/debug.txt
grep -q "You win" $S2E_LAST/debug.txt

# The number of states depends on how the compiler laid out the maze code:
# 401 with the gcc builds (Linux, MinGW), 1841 with clang on FreeBSD.
{% if 'freebsd' in project_name %}
EXPECTED_STATES=1841
{% else %}
EXPECTED_STATES=401
{% endif %}

COUNT=$(grep '\[State' "$S2E_LAST/debug.txt" | cut -d ' ' -f 3 | cut -d ']' -f 1 | sort -n | uniq | wc -l)
if [ $COUNT -ne $EXPECTED_STATES ]; then
    echo Incorrect number of states
    exit 1
fi

# Don't check coverage, it's unreliable with -O3, and we need O3.
# check_coverage {{project_name}} 70

s2e forkprofile {{ project_name }} > $S2E_LAST/forkprofile.txt
grep -q -i maze.c $S2E_LAST/forkprofile.txt
