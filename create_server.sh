#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

ENV=${1:-live}
FROM=${2:-live}   # label of volume to copy, e.g. "none", "live"

createServer "$ENV" "$FROM" || exit $?
