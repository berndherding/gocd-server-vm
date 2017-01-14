#!/usr/bin/env bash

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

STACK_SUFFIX=${1:-test}

GOCD_VOL="$(getStackname "gocd-vol" "$STACK_SUFFIX")"

destroyVolumes "$GOCD_VOL"
