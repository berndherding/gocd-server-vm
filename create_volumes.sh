#!/usr/bin/env bash

DIR="${BASH_SOURCE%/*}"
if [[ ! -d "$DIR" ]]; then DIR="$PWD"; fi

# shellcheck source=volumes.inc
. "$DIR/volumes.inc"

VOLUME_STACKNAME=gocd-vol
DEFAULT_LABEL=LIVE

_GO_PIPELINE_COUNTER=-${GO_PIPELINE_COUNTER:-0}

VOLUME_STACKNAME=$VOLUME_STACKNAME$_GO_PIPELINE_COUNTER

createVolumes \
  "${1:-$VOLUME_STACKNAME}" \
  "${2:-$DEFAULT_LABEL}"
