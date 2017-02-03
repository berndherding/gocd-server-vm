#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

ENV=${1:-live}
GOCD_URL=$2

switch "$ENV" "$GOCD_URL" || exit $?
