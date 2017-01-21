#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "$BASH_SOURCE")/volumes.inc"

# shellcheck source=switch.inc
. "$(dirname "$BASH_SOURCE")/switch.inc"

ENVLABEL=${1:-live}

switch "$ENVLABEL" "$GOCD_URL" || exit $?
