#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

env=${1:-test}

gocd_svc="$(getStackname "gocd-svc" "$env")"
gocd_svm="$(getStackname "gocd-svm" "$env")"
gocd_vol="$(getStackname "gocd-vol" "$env")"

destroyCluster  "$gocd_svc" || return $?
destroyMachine  "$gocd_svm" || return $?
destroyVolumes  "$gocd_vol" || return $?
