#!/usr/bin/env bash

DIR="${BASH_SOURCE%/*}"
if [[ ! -d "$DIR" ]]; then DIR="$PWD"; fi
_DIR="$DIR"

# shellcheck source=../server.inc
. "$DIR/../server.inc"
DIR="$_DIR"

# shellcheck source=../volumes.inc
. "$DIR/../volumes.inc"
DIR="$_DIR"

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
