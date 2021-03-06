#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

ENV=${1:-live}
FROM=${2:-live}   # label of volume to copy, e.g. "none", "live"

updateServer "$ENV" "$FROM" || exit $?
