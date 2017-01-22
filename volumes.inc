#!/bin/bash

[ ! -d "$(dirname "${BASH_SOURCE[0]}")/inc"    ] && ln -s ../gocd-base-ami/inc    "$(dirname "${BASH_SOURCE[0]}")/inc"
[ ! -d "$(dirname "${BASH_SOURCE[0]}")/target" ] && ln -s ../gocd-base-ami/target "$(dirname "${BASH_SOURCE[0]}")/target"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/commons.inc"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/keys.inc"

ERROR_getVolumeId_describeVolumes=1
ERROR_setSnapshotName_createTags=2
ERROR_getLabelOfVolume=12
ERROR_getLabeledVolumeName_describeVolumes=3
ERROR_getLabeledVolumeId_describeVolumes=4
ERROR_createSnapshot_createSnapshot=5
ERROR_createSnapshot_waitSnapshot=6
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



function getLabelOfVolume() {
  local volumename=$1

  # shellcheck disable=SC2016
  label=$(aws ec2 describe-volumes \
    --filters Name=tag:Name,Values="$volumename" \
    --query 'Volumes[0].Tags[?Key==`Stage`].Value' \
    --output text 2> /dev/null
  ) || return $ERROR_getLabelOfVolume

  [ "$label" != "None" ] && echo "$label" || echo ""
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



function getLabeledVolumeName() {
  local volumeId=$1

  # shellcheck disable=SC2016
  volumeName=$(aws ec2 describe-volumes \
    --filters Name=volume-id,Values="$volumeId" \
    --query 'Volumes[0].Tags[?Key==`Name`]|[0].Value' \
    --out text 2> /dev/null
  ) || return $ERROR_getLabeledVolumeName_describeVolumes

  [ "$volumeName" != "None" ] && echo "$volumeName" || echo ""
}



function createSnapshot() {
  local stackname=$1
  local volumename=$2
  local labeledVolumeId=$3

  local labeledVolumeName

  labeledVolumeName=$(getLabeledVolumeName "$labeledVolumeId")
  echo "* found $labeledVolumeName labeled \"$label\" ($labeledVolumeId). creating snapshot."

  unset snapshotId

  json=$(
    aws ec2 create-snapshot \
    --volume-id "$labeledVolumeId" \
    --description "based on $labeledVolumeName ($labeledVolumeId)"
  ) || return $ERROR_createSnapshot_createSnapshot

  # stupid, i know. just to not add a dependency to jq.
  snapshotId=${json##*SnapshotId\": \"}
  snapshotId=${snapshotId%%\"*}

  aws ec2 wait snapshot-completed --snapshot-ids "$snapshotId" \
  || return $ERROR_createSnapshot_waitSnapshot

  setSnapshotName "$snapshotId" "$stackname-$volumename" || return $?

  echo "* snapshotId for $volumename:  $snapshotId"
}



function createVolumeStack() {
  local stackname=$1
  local snapshotIdEtcGo=$2
  local snapshotIdVarLib=$3

  # NOTE remember to re-activate "DeletionPolicy" : "Snapshot" in vol.cf per parameter

  aws cloudformation create-stack \
  --stack-name "$stackname" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/volumes.cf" \
  --parameters \
    ParameterKey=SnapshotIdEtcGo,ParameterValue="$snapshotIdEtcGo" \
    ParameterKey=SnapshotIdVarLib,ParameterValue="$snapshotIdVarLib" \
  > /dev/null || return $ERROR_createVolumeStack_createStack

  echo "* waiting for $stackname to complete. this may take a few minutes."
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



function countVolumes() {
  local volumename=$1

  aws ec2 describe-volumes --filters Name=tag:Name,Values="$volumename" --query 'length(Volumes[*])' \
  || return $ERROR_countVolumes_describeVolumes
}



function createVolumes() {
  local label=$1
  local from=$2

  gocd_vol="$(getStackname "vol" "$label")"

  echo
  echo "createVolumes for $gocd_vol based on label \"$from\""

  count="$(countVolumes "$gocd_vol-etc-go")" || return $?

  if [ "$count" -eq 0 ] ; then
    echo "* no existing volumes. creating new ones."

    local origEtcGo snapEtcGo

    origEtcGo=$(getLabeledVolumeId "etc-go" "$from")

    if [ -n "$origEtcGo" ] ; then
      createSnapshot "$gocd_vol" "etc-go" "$origEtcGo"       || return $?
      snapEtcGo="$snapshotId"
    fi

    origVarLib=$(getLabeledVolumeId "var-lib" "$from")

    if [ -n "$origVarLib" ] ; then
      createSnapshot "$gocd_vol" "var-lib" "$origVarLib"     || return $?
      snapVarLib="$snapshotId"
    fi

    echo "* creating new volume stack $gocd_vol"
    createVolumeStack "$gocd_vol" "$snapEtcGo" "$snapVarLib" || return $?

  else
    echo "* volumes for $gocd_vol already exist. skipping."
  fi

  volumeIdEtcGo="$(getVolumeId "$gocd_vol-etc-go")"
  echo "* volumeId for etc-go:  $volumeIdEtcGo"

  volumeIdVarLib="$(getVolumeId "$gocd_vol-var-lib")"
  echo "* volumeId for var-lib: $volumeIdVarLib"
}



function destroyVolumes() {
  local label=$1

  gocd_vol="$(getStackname "vol" "$label")"

# TODO: if tagged "LIVE", make copies of volumes or use proper DeletionPolicy

  etcGo="$gocd_vol-etc-go"
  varLib="$gocd_vol-var-lib"

  labelEtcGo="$(getLabelOfVolume "$etcGo")"
  labelVarLib="$(getLabelOfVolume "$varLib")"

  if [ "$labelEtcGo" = "live" ] || [ "$labelVarLib" = "live" ] ; then
    return 0
  fi

  destroySnapshotOfVolume "$etcGo"  || return $? &
  destroySnapshotOfVolume "$varLib" || return $? &

  destroyStack "$gocd_vol"  || return $?
}
