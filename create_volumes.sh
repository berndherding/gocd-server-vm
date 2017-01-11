#!/usr/bin/env bash

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

env=${1:-test}
fromVolumeLabeled=${2:-none}  # "none", "live"

gocd_vol="$(getStackname "gocd-vol" "$env")"

createVolumesBasedOnLabel "$gocd_vol" "$fromVolumeLabeled"
