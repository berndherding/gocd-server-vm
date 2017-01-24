#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

ENVLABEL=${1:-live}

shopt -s extglob

destroyServer  "$ENVLABEL" || exit $?
destroyVolumes "$ENVLABEL" || exit $?
