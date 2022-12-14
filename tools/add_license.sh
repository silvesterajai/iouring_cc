#!/bin/bash

shopt -s globstar nullglob extglob
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# script to copy the headers to all the source files and header files
for f in **/*.@(cc|h); do
    if (grep Copyright $f); then
        echo "No need to copy the License Header to $f"
    else
        cat ${SCRIPT_DIR}/user.license $f >$f.new
        mv $f.new $f
        echo "License Header copied to $f"
    fi
done
