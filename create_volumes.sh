#!/usr/bin/env bash

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

VOLUME_STACKNAME=gocd-vol
DEFAULT_LABEL=LIVE

_GO_PIPELINE_COUNTER=-${GO_PIPELINE_COUNTER:-0}

VOLUME_STACKNAME=$VOLUME_STACKNAME$_GO_PIPELINE_COUNTER

createVolumesBasedOnVolumeLabel \
  "${1:-$VOLUME_STACKNAME}" \
  "${2:-$DEFAULT_LABEL}"
