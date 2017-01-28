#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "${BASH_SOURCE[0]}")/server.inc"

ENV=test-empty
FROM=no-volume
INSTANCE_TYPE=t2.small

SHUNIT=$(which shunit)



function testCreateServerWithEmptyVolumes() {
  createServer "$ENV" "$FROM" "$INSTANCE_TYPE"
  assertEquals "createServer failed" 0 $?
}



function testDestroyServer() {
  destroyServer "$ENV"
  assertEquals "destroyServer failed" 0 $?
}



# function oneTimeSetup()
# function oneTimeTearDown()
# function setup()
# function teardown()



# shellcheck source=/usr/local/bin/shunit
. "$SHUNIT"
