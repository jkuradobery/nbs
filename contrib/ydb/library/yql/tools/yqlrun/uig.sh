#!/bin/sh -e

#
# Usage: ./uig.sh PORT [--gdb]
#
# OPTIONS:
#     --gdb   run under gdb
#

SCRIPT_DIR="$(dirname $(readlink -f "$0"))"
UDFS_DIR="${SCRIPT_DIR}/../../udfs"
ASSETS_DIR=${SCRIPT_DIR}/http/www
GATEWAYS_CFG=${SCRIPT_DIR}/../../cfg/tests/gateways.conf

PORT=${1:-3000}

if [ "$2" = "--gdb" ]; then
    GDB="yag tool gdb --args"
fi

export LD_LIBRARY_PATH=.
${GDB} ${SCRIPT_DIR}/yqlrun ui \
    --udfs-dir ${UDFS_DIR} \
    --assets ${ASSETS_DIR} \
    --gateways-cfg ${GATEWAYS_CFG} \
    --remote --port $PORT
