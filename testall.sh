#!/bin/sh

for t in obj/test/*/*; do
    if ! $t > /dev/null; then
        echo "test failed: $t"
        exit $?
    fi
    echo "test passed: $t"
done
