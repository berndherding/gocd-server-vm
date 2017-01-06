#!/bin/bash

[ ! -d "$(dirname "${BASH_SOURCE[0]}")/inc"    ] && ln -s ../gocd-base-ami/inc    "$(dirname "${BASH_SOURCE[0]}")/inc"
[ ! -d "$(dirname "${BASH_SOURCE[0]}")/target" ] && ln -s ../gocd-base-ami/target "$(dirname "${BASH_SOURCE[0]}")/target"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/commons.inc"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/keys.inc"



function labelSnapshot() {
  local snapshotId=$1
  local name=$2

  aws ec2 create-tags \
  --resources "$snapshotId" \
  --tags Key=Name,Value="$name" \
  || return 5
}



function createSnapshotFromLive() {
  local stackname=$1
  local volumename=$2
  local liveVolumeDetails=$3

  liveVolumeId=$(getFromDetails "$liveVolumeDetails" VolumeId)
  liveVolumeName=$(getFromDetails "$liveVolumeDetails" VolumeName)

  json=$(
    aws ec2 create-snapshot \
    --volume-id "$liveVolumeId" \
    --description "based on $liveVolumeName ($liveVolumeId)"
  ) || return 3

  # stupid, i know. just to not add a dependency to jq.
  snapshotId=${json##*SnapshotId\": \"}
  snapshotId=${snapshotId%%\"*}

  aws ec2 wait snapshot-completed --snapshot-ids "$snapshotId" || return 4

  labelSnapshot "$snapshotId" "$stackname-$volumename"

  echo "$snapshotId"
}



function createVolumeStack() {
  local stackname=$1
  local snapshotIdEtcGo=$2
  local snapshotIdVarLib=$3

  # NOTE inconsistency: <region> defined here and in vol.cf. -> TODO template needs parameter
  # NOTE remember to re-activate "DeletionPolicy" : "Snapshot" in vol.cf per parameter

  aws cloudformation create-stack \
  --stack-name "$stackname" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/volumes.cf" \
  --parameters \
    ParameterKey=SnapshotIdEtcGo,ParameterValue="$snapshotIdEtcGo" \
    ParameterKey=SnapshotIdVarLib,ParameterValue="$snapshotIdVarLib" \
  || return 6

  echo "please wait for stack $stackname to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$stackname" --output text || return 7
}



function destroySnapshotOfVolume() {
  local volumename=$1

  snapshotId=$(aws ec2 describe-volumes --filters Name=tag:Name,Values="$volumename" --query 'Volumes[*].SnapshotId' --output text) || return 8

  if [ -n "$snapshotId" ] ; then
    aws ec2 delete-snapshot --snapshot-id "$snapshotId" || return 9
  fi
}



function getLabeledVolume() {
  local volumeBaseName=$1
  local label=$2
  local details

  # is there a volume labeled $volumeBaseName-* and tagged LIVE?
  # shellcheck disable=SC2016
  details=$(
    aws ec2 describe-volumes \
    --filters \
      Name=status,Values=available,in-use \
      Name=tag:Name,Values="*$volumeBaseName*" \
      Name=tag:Stage,Values=LIVE \
    --query 'Volumes[*].{VolumeName:Tags[?Key==`Name`].Value,VolumeId:VolumeId,State:State}' \
    --output json 2> /dev/null
  )

  details=${details#\[}
  details=${details%\]}

  echo "$details"
}



function getSnapshotId() {
  local stackname=$1 
  local volumename=$2
  local label=$3

  liveVolumeDetails="$(getLabeledVolume "$volumename" "$label")"

  if [ -n "$liveVolumeDetails" ] ; then
    snapshotId=$(createSnapshotFromLive "$stackname" "$volumename" "$liveVolumeDetails") || return $?
  fi

  # may be undefined
  echo "$snapshotId"
}



function createVolumes() {
  local stackname=$1
  local label=$2

  snapshotIdEtcGo=$(getSnapshotId  "$stackname" "etc-go"  "$label") || return $?
  snapshotIdVarLib=$(getSnapshotId "$stackname" "var-lib" "$label") || return $?

  createVolumeStack "$stackname" "$snapshotIdEtcGo" "$snapshotIdVarLib"

  getVolumeId "$stackname-etc-go"  > "$(dirname "${BASH_SOURCE[0]}")"/target/etcGoVolumeId
  getVolumeId "$stackname-var-lib" > "$(dirname "${BASH_SOURCE[0]}")"/target/varLibVolumeId
}



function destroyVolumes() {
  local stackname=$1

  # TODO: if tagged "LIVE", make copies of volumes

#  destroySnapshotOfVolume "$etcGoVolumeName"  || return $? &
#  destroySnapshotOfVolume "$varLibVolumeName" || return $? &

  destroyStack "$stackname"  || return $?
}
