#!/usr/bin/env bash

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

STACK_SUFFIX=${1:-test}
fromVolumeLabeled=${2:-none}  # "none", "live"

GOCD_VOL="$(getStackname "gocd-vol" "$STACK_SUFFIX")"

createVolumesBasedOnLabel "$GOCD_VOL" "$fromVolumeLabeled"
