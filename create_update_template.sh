#!/usr/bin/env bash

(
  awk '/"Resources" *: *{/ {print; p=1; next} !p' create-server.cf 
  cat update.cf 
  awk '/"Resources" *: *{/ {       p=1; next}  p' create-server.cf
) > "$(dirname "${BASH_SOURCE[0]}")/update-server.cf"
