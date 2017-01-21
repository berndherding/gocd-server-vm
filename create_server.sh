#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

STACK_SUFFIX=${1:-init}

from=${2:-live}   # label of volume to copy, e.g. "none", "live"

GOCD_VOL="$(getStackname "gocd-vol" "$STACK_SUFFIX")"
GOCD_SVM="$(getStackname "gocd-svm" "$STACK_SUFFIX")"

createVolumes "$GOCD_VOL" "$from" || exit $?
createServer  "$GOCD_SVM"         || exit $?
