#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "$BASH_SOURCE")/server.inc"

ENVIRONMENT=${1:-test}

_GO_PIPELINE_COUNTER=-${GO_PIPELINE_COUNTER:-0}

SERVER_STACKNAME=gocd-svm
SERVER_STACKNAME=$SERVER_STACKNAME$_GO_PIPELINE_COUNTER
SERVER_STACKNAME=$SERVER_STACKNAME-$ENVIRONMENT

CLUSTER_STACKNAME=gocd-svc
CLUSTER_STACKNAME=$CLUSTER_STACKNAME$_GO_PIPELINE_COUNTER
CLUSTER_STACKNAME=$CLUSTER_STACKNAME-$ENVIRONMENT

destroyServerInstance \
  "$SERVER_STACKNAME" \
  "$CLUSTER_STACKNAME"
