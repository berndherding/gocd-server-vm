#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

STACK_SUFFIX=${1:-init}
DESTROY_VOLUMES=${2:-false}

shopt -s extglob

GOCD_SVC="$(getStackname "gocd-svc" "$STACK_SUFFIX")"
GOCD_SVM="$(getStackname "gocd-svm" "$STACK_SUFFIX")"
GOCD_VOL="$(getStackname "gocd-vol" "$STACK_SUFFIX")"

destroyCluster  "$GOCD_SVC" || exit $?
destroyMachine  "$GOCD_SVM" || exit $?

[ "$DESTROY_VOLUMES" = "destroy-volumes" ] || exit 0

destroyVolumes  "$GOCD_VOL" || exit $?
