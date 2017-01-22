#!/usr/bin/env bash

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

ENVLABEL=${1:-test}
FROM=${2:-none}       # "none", "live"

createVolumes "$ENVLABEL" "$FROM" || exit $?
