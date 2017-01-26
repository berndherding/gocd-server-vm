#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "${BASH_SOURCE[0]}")/server.inc"

ENV=$1
FROM=$2
INSTANCE_TYPE=$3

SHUNIT=$(which shunit)



function testCreateServer() {
  createServer "$ENV" no-master-volume "$INSTANCE_TYPE"
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
