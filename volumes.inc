#!/bin/bash

shopt -s extglob

DIR="${BASH_SOURCE%/*}"
if [[ ! -d "$DIR" ]]; then DIR="$PWD"; fi

[ ! -d "$DIR/inc"    ] && ln -s ../gocd-base-ami/inc    "$DIR/inc"
[ ! -d "$DIR/target" ] && ln -s ../gocd-base-ami/target "$DIR/target"

# shellcheck source=/dev/null
. "$DIR/inc/commons.inc"

# shellcheck source=/dev/null
. "$DIR/inc/keys.inc"



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
  --template-body file://"$DIR/volumes.cf" \
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



function getLiveOrMasterVolume() {
  local volumeBaseName=$1
  local details

  # is there a volume labeled $volumeBaseName-* and tagged LIVE?
  # shellcheck disable=SC2016
  details=$(
    aws ec2 describe-volumes \
    --filters \
      Name=status,Values=available,in-use \
      Name=tag:Name,Values="*$volumeBaseName-*" \
      Name=tag:Stage,Values=LIVE \
    --query 'Volumes[*].{VolumeName:Tags[?Key==`Name`].Value,VolumeId:VolumeId,State:State}' \
    --output json 2> /dev/null
  )

  details=${details#\[}
  details=${details%\]}

  if [ -z "$details" ] ; then

    # is there a volume labeled $volumeBaseName (without counter and prefix)?
    # shellcheck disable=SC2016
    details=$(
      aws ec2 describe-volumes \
      --filters \
        Name=status,Values=available,in-use \
        Name=tag:Name,Values="$volumeBaseName" \
      --query 'Volumes[*].{VolumeName:Tags[?Key==`Name`].Value,VolumeId:VolumeId,State:State}' \
      --output json 2> /dev/null
    )
  fi

  details=${details#\[}
  details=${details%\]}

  echo "$details"
}



function getSnapshotId() {
  local stackname=$1 
  local volumename=$2

  liveVolumeDetails="$(getLiveOrMasterVolume "$(getBaseName "$volumename")")"

  if [ -n "$liveVolumeDetails" ] ; then
    snapshotId=$(createSnapshotFromLive "$stackname" "$volumename" "$liveVolumeDetails") || return $?
    labelSnapshot "$snapshotId" "$stackname-$volumename"
  fi

  echo "$snapshotId"
}



function createVolumes() {
  # TODO
  # shellcheck disable=SC2034
  local label=$1
  local stackname=$2

  snapshotIdEtcGo=$(getSnapshotId  "$stackname" "etc-go")  || return $?
  snapshotIdVarLib=$(getSnapshotId "$stackname" "var-lib") || return $?

  createVolumeStack "$stackname" "$snapshotIdEtcGo" "$snapshotIdVarLib"

  getVolumeId "$stackname-etc-go"  > "$DIR"/target/etcGoVolumeId
  getVolumeId "$stackname-var-lib" > "$DIR"/target/varLibVolumeId
}



function destroyVolumes() {
  local stackname=$1

  # TODO: if tagged "LIVE", make copies of volumes

#  destroySnapshotOfVolume "$etcGoVolumeName"  || return $? &
#  destroySnapshotOfVolume "$varLibVolumeName" || return $? &

  destroyStack "$stackname"  || return $?
}
