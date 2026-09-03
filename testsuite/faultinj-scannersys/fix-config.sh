#!/bin/sh
set -e

echo "Patching bootstrap.sh to start the driver..."
sed -i.bak 's/# sc start my_driver_service/sc start scanner/g' $PROJECT_DIR/bootstrap.sh && rm -f $PROJECT_DIR/bootstrap.sh.bak
sed -i.bak 's/sleep 30/sleep 5/g' $PROJECT_DIR/bootstrap.sh && rm -f $PROJECT_DIR/bootstrap.sh.bak
# Simulate DFS
echo "Patching s2e-config.lua..."
sed -i.bak 's/batchTime = 5/batchTime = 5000/g' $PROJECT_DIR/s2e-config.lua && rm -f $PROJECT_DIR/s2e-config.lua.bak