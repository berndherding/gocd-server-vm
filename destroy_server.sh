#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

ENVLABEL=${1:-live}
DESTROY_VOLUMES=${2:-do-not-destroy-volumes}

shopt -s extglob

destroyServer  "$ENVLABEL" || exit $?

[ "$DESTROY_VOLUMES" = "destroy-volumes" ] || exit 0

destroyVolumes "$ENVLABEL" || exit $?
