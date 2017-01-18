#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "${BASH_SOURCE[0]}")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "${BASH_SOURCE[0]}")/volumes.inc"

env=test

VOLUMES_STACKNAME="$(getStackname "gocd-vol" "$env")"
SERVER_STACKNAME="$(getStackname "gocd-svm" "$env")"

SHUNIT=$(which shunit)



function testCreateVolumes() {
  # "NONE" label should not exist -> new, empty volume created
  createVolumesBasedOnLabel "$VOLUMES_STACKNAME" "NONE"
  assertEquals "createVolumes failed" 0 $?
}



# volumes are empty
function testCreateServer() {
  createServer "$SERVER_STACKNAME"
  assertEquals "createServer failed" 0 $?
}



function testDestroyServer() {
  destroyServer "$SERVER_STACKNAME"
  assertEquals "destroyServer failed" 0 $?
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
