#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

STACK_SUFFIX=${1:-init}

fromVolumeLabel=${2:-live}                 # "none", "live"
mapHostVolumes=${3:-map-host-volumes}

GOCD_VOL="$(getStackname "gocd-vol" "$STACK_SUFFIX")"
GOCD_SVM="$(getStackname "gocd-svm" "$STACK_SUFFIX")"

createVolumes "$GOCD_VOL" "$fromVolumeLabel" || exit $?
createServer  "$GOCD_SVM" "$mapHostVolumes"  || exit $?
