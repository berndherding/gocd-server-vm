#!/usr/bin/env bash

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

ENVLABEL=${1:-test}

destroyVolumes "$ENVLABEL" || exit $?
