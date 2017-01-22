#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "${BASH_SOURCE[0]}")/server.inc"

# shellcheck source=volumes.inc
. "$(dirname "${BASH_SOURCE[0]}")/volumes.inc"

ENVLABEL=test

SHUNIT=$(which shunit)



function testCreateVolumes() {
  # label "no-label" should not exist -> new, empty volume created
  createVolumes "$ENVLABEL" "no-label"
  assertEquals "createVolumes failed" 0 $?
}



# volumes are empty
function testCreateServer() {
  createServer "$ENVLABEL"
  assertEquals "createServer failed" 0 $?
}



function testDestroyServer() {
  destroyServer "$ENVLABEL"
  assertEquals "destroyServer failed" 0 $?
}



function testDestroyVolumes() {
  destroyVolumes "$ENVLABEL"
  assertEquals "destroyVolumes failed" 0 $?
}



# function oneTimeSetup()
# function oneTimeTearDown()
# function setup()
# function teardown()



# shellcheck source=/usr/local/bin/shunit
. "$SHUNIT"
