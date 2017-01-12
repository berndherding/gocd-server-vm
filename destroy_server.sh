#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

ENV=${1:-init}
DESTROY_VOLUMES=${2:-false}

GOCD_SVC="$(getStackname "gocd-svc" "$ENV")"
GOCD_SVM="$(getStackname "gocd-svm" "$ENV")"
GOCD_VOL="$(getStackname "gocd-vol" "$ENV")"

destroyCluster  "$GOCD_SVC" || return $?
destroyMachine  "$GOCD_SVM" || return $?

[ "$DESTROY_VOLUMES" = "destroy-volumes" ] || return

destroyVolumes  "$GOCD_VOL" || return $?
