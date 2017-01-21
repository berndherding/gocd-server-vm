#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "${BASH_SOURCE[0]}")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "${BASH_SOURCE[0]}")/volumes.inc"

STACK_SUFFIX=test

GOCD_VOL="$(getStackname "gocd-vol" "$STACK_SUFFIX")"
GOCD_SVM="$(getStackname "gocd-svm" "$STACK_SUFFIX")"

SHUNIT=$(which shunit)



function testCreateVolumes() {
  # label "no-label" should not exist -> new, empty volume created
  createVolumes "$GOCD_VOL" "no-label"
  assertEquals "createVolumes failed" 0 $?
}



# volumes are empty
function testCreateServer() {
  createServer "$GOCD_SVM"
  assertEquals "createServer failed" 0 $?
}



function testDestroyServer() {
  destroyServer "$GOCD_SVM"
  assertEquals "destroyServer failed" 0 $?
}



function testDestroyVolumes() {
  destroyVolumes "$GOCD_VOL"
  assertEquals "destroyVolumes failed" 0 $?
}



# function oneTimeSetup()
# function oneTimeTearDown()
# function setup()
# function teardown()



# shellcheck source=/usr/local/bin/shunit
. "$SHUNIT"
