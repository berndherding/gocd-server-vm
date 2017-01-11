#!/usr/bin/env bash

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

env=${1:-test}

gocd_vol="$(getStackname "gocd-vol" "$env")"

destroyVolumes "$gocd_vol"
