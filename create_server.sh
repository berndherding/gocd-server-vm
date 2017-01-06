#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

SERVER_STACKNAME=gocd-svm
CLUSTER_STACKNAME=gocd-svc

_GO_PIPELINE_COUNTER=-${GO_PIPELINE_COUNTER:-0}

SERVER_STACKNAME=$SERVER_STACKNAME$_GO_PIPELINE_COUNTER
CLUSTER_STACKNAME=$CLUSTER_STACKNAME$_GO_PIPELINE_COUNTER

createServerInstance \
  "${1:-$SERVER_STACKNAME}" \
  "${2:-$CLUSTER_STACKNAME}"
