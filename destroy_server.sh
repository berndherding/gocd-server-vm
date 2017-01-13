#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

STACK_SUFFIX=${1:-init-0}
DESTROY_VOLUMES=${2:-false}

shopt -s extglob

ENV=
if [ -z "${STACK_SUFFIX//+([0-9])}" ] ; then
  GO_PIPELINE_COUNTER="$STACK_SUFFIX"
else
  ENV=${STACK_SUFFIX##+([0-9])-}
  if [ -n "$ENV" ] ; then
    GO_PIPELINE_COUNTER=${STACK_SUFFIX%%$ENV}
    GO_PIPELINE_COUNTER=${GO_PIPELINE_COUNTER%%-}
  fi
fi

#echo "ENV \"$ENV\" GO_PIPELINE_COUNTER \"$GO_PIPELINE_COUNTER\""

GOCD_SVC="$(getStackname "gocd-svc" "$ENV")"
GOCD_SVM="$(getStackname "gocd-svm" "$ENV")"
GOCD_VOL="$(getStackname "gocd-vol" "$ENV")"

destroyCluster  "$GOCD_SVC" || exit $?
destroyMachine  "$GOCD_SVM" || exit $?

[ "$DESTROY_VOLUMES" = "destroy-volumes" ] || exit 0

destroyVolumes  "$GOCD_VOL" || exit $?
