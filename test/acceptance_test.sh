#!/usr/bin/env bash

# shellcheck source=../server.inc
. "$(dirname "$BASH_SOURCE")/../server.inc"

# shellcheck source=../volumes.inc
. "$(dirname "$BASH_SOURCE")/../volumes.inc"

SERVER_STACKNAME=test-svm
CLUSTER_STACKNAME=test-svc
VOLUMES_STACKNAME=test-vol

_GO_PIPELINE_COUNTER=-${GO_PIPELINE_COUNTER:-0}

SERVER_STACKNAME=$SERVER_STACKNAME$_GO_PIPELINE_COUNTER
CLUSTER_STACKNAME=$CLUSTER_STACKNAME$_GO_PIPELINE_COUNTER
VOLUMES_STACKNAME=$VOLUMES_STACKNAME$_GO_PIPELINE_COUNTER

SHUNIT=$(which shunit)



function testCreateVolumes() {
  createVolumes "$VOLUMES_STACKNAME" "TEST"
  assertEquals "createVolumes failed" 0 $?
}



function testDestroyVolumes() {
  destroyVolumes "$VOLUMES_STACKNAME"
  assertEquals "destroyVolumes failed" 0 $?
}



# function oneTimeSetup()
# function oneTimeTearDown()
# function setup()
# function teardown()



# shellcheck source=/usr/local/bin/shunit
. "$SHUNIT"
