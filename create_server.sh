#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

env=${1:-test}
fromVolumeLabeled=${2:-none}  # "none", "live"
mapSourceVolumes=${3:-true}

gocd_vol="$(getStackname "gocd-vol" "$env")"
gocd_svm="$(getStackname "gocd-svm" "$env")"
gocd_svc="$(getStackname "gocd-svc" "$env")"

createVolumesBasedOnLabel "$gocd_vol" "$fromVolumeLabeled" || return $?
createMachine "$gocd_svm" || return $?
createCluster "$gocd_svm" "$gocd_svc" "$mapSourceVolumes"  || return $?
