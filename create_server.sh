#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

STACK_SUFFIX=${1:-init}
fromVolumeLabeled=${2:-none}   # "none", "live"
mapSourceVolumes=${3:-true}

GOCD_VOL="$(getStackname "gocd-vol" "$STACK_SUFFIX")"
GOCD_SVM="$(getStackname "gocd-svm" "$STACK_SUFFIX")"

createVolumesBasedOnLabel "$GOCD_VOL" "$fromVolumeLabeled" || exit $?
createServer              "$GOCD_SVM" "$mapSourceVolumes"  || exit $?
