#!/bin/sh
set -e

echo "Patching s2e-config.lua..."

# Frequent state switching slows down large guests, increase batch time to avoid that
# -i.bak works with both GNU and BSD sed
sed -i.bak 's/batchTime = 5/batchTime = 5000/g' $PROJECT_DIR/s2e-config.lua && rm -f $PROJECT_DIR/s2e-config.lua.bak

# Make sed worked
grep -q "batchTime = 5000" $PROJECT_DIR/s2e-config.lua
