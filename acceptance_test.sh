#!/usr/bin/env bash

# shellcheck source=server.inc
. "$(dirname "${BASH_SOURCE[0]}")/server.inc"

ENV=${1:-test}
FROM=${2:-no-origin}
INSTANCE_TYPE=${3-t2.small}

SHUNIT=$(which shunit)



function testCreateServer() {
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
