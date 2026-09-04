#!/bin/bash
ARGS=("$@")
echo "First arg: ${ARGS[0]}"
for arg in "$@"; do
    echo $arg
done

trap "echo Error!" ERR
