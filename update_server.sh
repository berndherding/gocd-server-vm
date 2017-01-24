#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

ENVLABEL=${1:-live}

updateServer "$ENVLABEL" || exit $?
