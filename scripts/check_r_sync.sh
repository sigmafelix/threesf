#!/usr/bin/env sh
# Verify that the R package's vendored core matches include/ and src/.
set -e
for f in threesf.hpp constructive.hpp io.hpp threesf_c.h; do cmp -s core/include/threesf/$f r/threesf/inst/include/threesf/$f || { echo "r/threesf/inst/include/threesf/$f is stale: run 'make -C core r-sync'"; exit 1; }; done
cmp -s core/src/threesf_c.cpp r/threesf/src/threesf_c.cpp || { echo "r/threesf/src/threesf_c.cpp is stale: run 'make -C core r-sync'"; exit 1; }
echo "R vendored sources in sync"
