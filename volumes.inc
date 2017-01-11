#!/bin/bash

[ ! -d "$(dirname "${BASH_SOURCE[0]}")/inc"    ] && ln -s ../gocd-base-ami/inc    "$(dirname "${BASH_SOURCE[0]}")/inc"
[ ! -d "$(dirname "${BASH_SOURCE[0]}")/target" ] && ln -s ../gocd-base-ami/target "$(dirname "${BASH_SOURCE[0]}")/target"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/commons.inc"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/keys.inc"



function setSnapshotName() {
  local snapshotId=$1
  local snapshotName=$2

  aws ec2 create-tags \
  --resources "$snapshotId" \
  --tags Key=Name,Value="$snapshotName" \
  || return 5
}



function createSnapshotFromLabeledVolume() {
  local stackname=$1
  local volumename=$2
  local labeledVolumeDetails=$3

  labeledVolumeId=$(getFromDetails "$labeledVolumeDetails" VolumeId)
  labeledVolumeName=$(getFromDetails "$labeledVolumeDetails" VolumeName)

  json=$(
    aws ec2 create-snapshot \
    --volume-id "$labeledVolumeId" \
    --description "based on $labeledVolumeName ($labeledVolumeId)"
  ) || return 3

  # stupid, i know. just to not add a dependency to jq.
  snapshotId=${json##*SnapshotId\": \"}
  snapshotId=${snapshotId%%\"*}

  aws ec2 wait snapshot-completed --snapshot-ids "$snapshotId" || return 4

  setSnapshotName "$snapshotId" "$stackname-$volumename"

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
  local volumeLabel=$2
  local details

  # is there a volume named $*volumeBaseName* and tagged with $volumeLabel?
  # shellcheck disable=SC2016
  details=$(
    aws ec2 describe-volumes \
    --filters \
      Name=status,Values=available,in-use \
      Name=tag:Stage,Values="$volumeLabel" \
      Name=tag:Name,Values="*$volumeBaseName*" \
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
  local basedOnLabel=$3

  labeledVolumeDetails="$(getLabeledVolume "$volumename" "$basedOnLabel")"

  if [ -n "$labeledVolumeDetails" ] ; then
    snapshotId=$(createSnapshotFromLabeledVolume "$stackname" "$volumename" "$labeledVolumeDetails") || return $?
  fi

  # may be undefined
  echo "$snapshotId"
}



function createVolumesBasedOnLabel() {
  local stackname=$1
  local basedOnLabel=$2

  snapshotIdEtcGo=$(getSnapshotId  "$stackname" "etc-go"  "$basedOnLabel") || return $?
  snapshotIdVarLib=$(getSnapshotId "$stackname" "var-lib" "$basedOnLabel") || return $?

  createVolumeStack "$stackname" "$snapshotIdEtcGo" "$snapshotIdVarLib"    || return $?

  getVolumeId "$stackname-etc-go"  > "$(dirname "${BASH_SOURCE[0]}")"/target/etcGoVolumeId
  getVolumeId "$stackname-var-lib" > "$(dirname "${BASH_SOURCE[0]}")"/target/varLibVolumeId
}



function destroyVolumes() {
  local stackname=$1

  # TODO: if tagged "LIVE", make copies of volumes or use proper DeletionPolicy

#  destroySnapshotOfVolume "$etcGoVolumeName"  || return $? &
#  destroySnapshotOfVolume "$varLibVolumeName" || return $? &

  destroyStack "$stackname"  || return $?
}
