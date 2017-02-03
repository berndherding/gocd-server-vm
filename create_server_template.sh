#!/usr/bin/env bash

(
  awk '/"Resources" *: *{/ {print; p=1; next} !p' vm.cf 
  cat service.cf 
  awk '/"Resources" *: *{/ {       p=1; next}  p' vm.cf
) > "$(dirname "${BASH_SOURCE[0]}")/server.cf"
