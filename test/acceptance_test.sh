#!/usr/bin/env bash

# shellcheck source=../server.inc
. "$(dirname "${BASH_SOURCE[0]}")/../server.inc"

# shellcheck source=../volumes.inc
. "$(dirname "${BASH_SOURCE[0]}")/../volumes.inc"

SERVER_STACKNAME=gocd-svm
CLUSTER_STACKNAME=gocd-svc
VOLUMES_STACKNAME=gocd-vol

_GO_PIPELINE_COUNTER=-${GO_PIPELINE_COUNTER:-0}

SERVER_STACKNAME=$SERVER_STACKNAME$_GO_PIPELINE_COUNTER
CLUSTER_STACKNAME=$CLUSTER_STACKNAME$_GO_PIPELINE_COUNTER
VOLUMES_STACKNAME=$VOLUMES_STACKNAME$_GO_PIPELINE_COUNTER

SHUNIT=$(which shunit)



function testCreateVolumes() {
  createVolumesBasedOnVolumeLabel "$VOLUMES_STACKNAME" "TEST"
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
