#!/usr/bin/env bash

DIR="${BASH_SOURCE%/*}"
if [[ ! -d "$DIR" ]]; then DIR="$PWD"; fi

# shellcheck source=volumes.inc
. "$DIR/volumes.inc"

DEFAULT_LABEL=LIVE
VOLUME_STACKNAME=gocd-vol

_GO_PIPELINE_COUNTER=-${GO_PIPELINE_COUNTER:-0}

VOLUME_STACKNAME=$VOLUME_STACKNAME$_GO_PIPELINE_COUNTER

createVolumes \
  "${1:-$DEFAULT_LABEL}" \
  "${2:-$VOLUME_STACKNAME}"
