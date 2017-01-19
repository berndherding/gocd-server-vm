#!/bin/bash

[ ! -d "$(dirname "${BASH_SOURCE[0]}")/inc"    ] && ln -s ../gocd-base-ami/inc    "$(dirname "${BASH_SOURCE[0]}")/inc"
[ ! -d "$(dirname "${BASH_SOURCE[0]}")/target" ] && ln -s ../gocd-base-ami/target "$(dirname "${BASH_SOURCE[0]}")/target"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/commons.inc"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/keys.inc"

ERROR_getVolumeId_describeVolumes=1
ERROR_setSnapshotName_createTags=2
ERROR_getLabeledVolumeName_describeVolumes=3
ERROR_getLabeledVolumeId_describeVolumes=4
ERROR_createSnapshotFromVolume_createSnapshot=5
ERROR_createSnapshotFromVolume_waitSnapshot=6
ERROR_createVolumeStack_createStack=7
ERROR_createVolumeStack_waitStack=8
ERROR_destroySnapshotOfVolume_describeVolumes=9
ERROR_destroySnapshotOfVolume_deleteSnaphot=10
ERROR_countVolumes_describeVolumes=11



function getVolumeId() {
  local goVolumeName=$1
  local status=${2:-available}

  aws ec2 describe-volumes \
  --filters Name=status,Values="$status" Name=tag:Name,Values="$goVolumeName" \
  --query "Volumes[*].VolumeId" \
  --output text 2> /dev/null \
  || return $ERROR_getVolumeId_describeVolumes
}



function setSnapshotName() {
  local snapshotId=$1
  local snapshotName=$2

  aws ec2 create-tags \
  --resources "$snapshotId" \
  --tags Key=Name,Value="$snapshotName" \
  || return $ERROR_setSnapshotName_createTags
}



function getLabeledVolumeName() {
  local volumeName=$1
  local volumeLabel=$2

  # shellcheck disable=SC2016
  volumeName=$(aws ec2 describe-volumes \
    --filters \
      Name=status,Values=available,in-use \
      Name=tag:Stage,Values="$volumeLabel" \
      Name=tag:Name,Values="*$volumeName*" \
    --query 'Volumes[0].Tags[?Key==`Name`]|[0].Value' \
    --out text 2> /dev/null
  ) || return $ERROR_getLabeledVolumeName_describeVolumes

  [ "$volumeName" != "None" ] && echo "$volumeName" || echo ""
}



function getLabeledVolumeId() {
  local volumeName=$1
  local volumeLabel=$2

  volumeId=$(aws ec2 describe-volumes \
    --filters \
      Name=status,Values=available,in-use \
      Name=tag:Stage,Values="$volumeLabel" \
      Name=tag:Name,Values="*$volumeName*" \
    --query 'Volumes[0].VolumeId' \
    --out text 2> /dev/null
  ) || return $ERROR_getLabeledVolumeId_describeVolumes

  [ "$volumeId" != "None" ] && echo "$volumeId" || echo ""
}



function createSnapshotFromVolume() {
  local stackname=$1
  local volumename=$2
  local labeledVolumeName=$3
  local labeledVolumeId=$4

  json=$(
    aws ec2 create-snapshot \
    --volume-id "$labeledVolumeId" \
    --description "based on $labeledVolumeName ($labeledVolumeId)"
  ) || return $ERROR_createSnapshotFromVolume_createSnapshot

  # stupid, i know. just to not add a dependency to jq.
  snapshotId=${json##*SnapshotId\": \"}
  snapshotId=${snapshotId%%\"*}

  aws ec2 wait snapshot-completed --snapshot-ids "$snapshotId" \
  || return $ERROR_createSnapshotFromVolume_waitSnapshot

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
  || return $ERROR_createVolumeStack_createStack

  echo "please wait for stack $stackname to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$stackname" --output text \
  || return $ERROR_createVolumeStack_waitStack
}



function destroySnapshotOfVolume() {
  local volumename=$1

  snapshotId=$(aws ec2 describe-volumes \
    --filters Name=tag:Name,Values="$volumename" \
    --query 'Volumes[*].SnapshotId' \
    --output text
  ) || return $ERROR_destroySnapshotOfVolume_describeVolumes

  if [ -n "$snapshotId" ] ; then
    aws ec2 delete-snapshot --snapshot-id "$snapshotId" \
    || return $ERROR_destroySnapshotOfVolume_deleteSnaphot
  fi
}



function getSnapshotId() {
  local stackname=$1 
  local volumename=$2
  local label=$3

  labeledVolumeName=$(getLabeledVolumeName "$volumename" "$label")
  labeledVolumeId=$(getLabeledVolumeId "$volumename" "$label")

  local snapshotId

  if [ -n "$labeledVolumeId" ] ; then
    snapshotId=$(createSnapshotFromVolume \
      "$stackname" \
      "$volumename" \
      "$labeledVolumeName" \
      "$labeledVolumeId"
    ) || return $?
  fi

  # may be undefined
  echo "$snapshotId"
}



function countVolumes() {
  local volumename=$1

  aws ec2 describe-volumes --filters Name=tag:Name,Values="$volumename" --query 'length(Volumes[*])' \
  || return $ERROR_countVolumes_describeVolumes
}



function createVolumes() {
  local stackname=$1
  local label=$2

  count="$(countVolumes "$stackname-etc-go")" || return $?

  if [ "$count" -eq 0 ] ; then
    snapshotIdEtcGo=$(getSnapshotId  "$stackname" "etc-go"  "$label")     || return $?
    snapshotIdVarLib=$(getSnapshotId "$stackname" "var-lib" "$label")     || return $?
    createVolumeStack "$stackname" "$snapshotIdEtcGo" "$snapshotIdVarLib" || return $?
  fi

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
