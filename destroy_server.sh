#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

ENV=${1:-live}

destroyServer "$ENV" || exit $?
