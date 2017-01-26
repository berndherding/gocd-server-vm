#!/usr/bin/env bash

[ ! -d "$(dirname "${BASH_SOURCE[0]}")/inc"    ] && ln -s ../gocd-base-ami/inc    "$(dirname "${BASH_SOURCE[0]}")/inc"
[ ! -d "$(dirname "${BASH_SOURCE[0]}")/target" ] && ln -s ../gocd-base-ami/target "$(dirname "${BASH_SOURCE[0]}")/target"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/commons.inc"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/keys.inc"

ERROR_createServer_createStack=1
ERROR_createServer_waitStack=2
ERROR_getPublicIpAddress=3
ERROR_getCertificateArn=4
ERROR_noDockerHubUsername=5
ERROR_noDockerHubPassword=6
ERROR_noDockerHubEmail=7
ERROR_updateServer_updateStack=8
ERROR_udpateServer_waitStack=9
ERROR_getSnapshotId=10
ERROR_getInstanceId=11
ERROR_getVolumeId=12
ERROR_setResourceTag=13
ERROR_findVolumesWithoutName=14
ERROR_getVolumeIdsByNameAndEnv=15
ERROR_createSnapshot=16
ERROR_waitCreateSnapshot=17
ERROR_getVolumeName=18



function getSnapshotId() {
  local from=$1
  local pattern=$2

  aws ec2 describe-snapshots \
  --owner-ids self \
  --filters \
    Name=tag:Name,Values="*$pattern*" \
    Name=tag:Env,Values="$from" \
  --query 'Snapshots[*].SnapshotId' \
  --output text || return $ERROR_getSnapshotId
}



function getInstanceId() {
  local gocd_svm=$1

  aws ec2 describe-instances \
  --filters \
    Name=tag:Name,Values="$gocd_svm" \
    Name=instance-state-name,Values=running,pending \
  --query 'Reservations[*].Instances[*].InstanceId' \
  --output text || return $ERROR_getInstanceId
}



function getVolumeIdByPatternAndEnv() {
  local pattern=$1
  local env=$2

  volumeId=$(aws ec2 describe-volumes \
    --filters \
      Name=status,Values=available,in-use \
      Name=tag:Name,Values="*$pattern*" \
      Name=tag:Env,Values="$env" \
    --query 'Volumes[0].VolumeId' \
    --output text 
  ) || return $ERROR_getVolumeIdsByNameAndEnv

  [ "$volumeId" != "None" ] && echo "$volumeId" || echo ""
}



function getVolumeId() {
  local instanceId=$1
  local devicename=$2

  volumeId=$(aws ec2 describe-volumes \
    --filters \
      Name=attachment.instance-id,Values="$instanceId" \
      Name=attachment.device,Values="$devicename" \
    --query 'Volumes[*].VolumeId' \
    --output text 
  ) || return $ERROR_getVolumeId

  [ "$volumeId" != "None" ] && echo "$volumeId" || echo ""
}



function getVolumeName() {
  local volumeId=$1

  # shellcheck disable=SC2016
  aws ec2 describe-volumes \
  --volume-ids "$volumeId" \
  --query 'Volumes[*].Tags[?Key==`Name`].Value' \
  --output text || return $ERROR_getVolumeName
}



function createSnapshot() {
  local snapshotName=$1
  local fromVolumeId=$2

  fromVolumeName=$(getVolumeName "$fromVolumeId")

  snapshotId=$(aws ec2 create-snapshot \
    --volume-id="$fromVolumeId" \
    --description "Created by create_server.sh for $snapshotName from $fromVolumeName ($fromVolumeId)" \
    --query SnapshotId \
    --output text
  ) || return $ERROR_createSnapshot

  sleep 10  # https://forums.aws.amazon.com/thread.jspa?messageID=564227, JoshuaF@AWS Posted on: Aug 19, 2014 7:18 AM

  aws ec2 wait snapshot-completed --snapshot-ids "$snapshotId" \
  || return $ERROR_waitCreateSnapshot

  setResourceTag "$snapshotId" "Name" "$snapshotName" || return $?

  echo "$snapshotId"
}



function getPublicIpAddress() {
  local stackname=$1

  aws ec2 describe-instances \
  --filters \
    "Name=tag:Name,Values=$stackname" \
    "Name=instance-state-name,Values=running" \
  --query "Reservations[*].Instances[*].PublicIpAddress" \
  --output text || return $ERROR_getPublicIpAddress
}



function getCertificateArn() {
  local fqdn=$1

  aws acm list-certificates --query 'CertificateSummaryList[?DomainName==`'"$fqdn"'`].CertificateArn' --output text \
  || return $ERROR_getCertificateArn
}



function setResourceTag() {
  local id=$1
  local key=$2
  local value=$3

  aws ec2 create-tags \
  --resources "$id" \
  --tags Key="$key",Value="$value" \
  || return $ERROR_setResourceTag
}



function setVolumeNames() {
  local gocd_svm=$1

  instanceId="$(getInstanceId "$gocd_svm")"                 || return $?
  volumeIdXvdb="$(getVolumeId "$instanceId" "/dev/xvdb")"   || return $?
  volumeIdXvdc="$(getVolumeId "$instanceId" "/dev/xvdc")"   || return $?
  setResourceTag "$volumeIdXvdb" "Name" "$gocd_svm-etc-go"  || return $?
  setResourceTag "$volumeIdXvdc" "Name" "$gocd_svm-var-lib" || return $?
  setVolumesWithoutName "$instanceId" "$gocd_svm"           || return $?
}



function setVolumesWithoutName() {
  local instanceId=$1
  local gocd_svm=$2

  # shellcheck disable=SC2016
  volumeDeviceTuples=$(aws ec2 describe-volumes \
    --filters Name=attachment.instance-id,Values="$instanceId" \
    --query '
      Volumes[*].{
        VolumeId: VolumeId,
        Device: Attachments[0].Device,
        Name:Tags[?Key==`Name`].Value|[0]
      }
      | [?(Name==`null`||Name==``)].[VolumeId,Device]' \
    --output text \
  ) || return $ERROR_findVolumesWithoutName

  IFS=$'\n'
  for volumeDeviceTuple in $volumeDeviceTuples ; do
    unset IFS
    a=($volumeDeviceTuple)
    volumeId=${a[0]}
    device=${a[1]}
    device=${device//\//-}
    device=${device#-}
    setResourceTag "$volumeId" "Name" "$gocd_svm-$device" || return $?
  done
}



function createServer() {
  local env=$1
  local from=$2
  local instanceType=$3

  [ -z "$DOCKER_HUB_USERNAME" ] && return $ERROR_noDockerHubUsername
  [ -z "$DOCKER_HUB_PASSWORD" ] && return $ERROR_noDockerHubPassword
  [ -z "$DOCKER_HUB_EMAIL" ]    && return $ERROR_noDockerHubEmail

  gocd_svm="$(getStackname "svm" "$env")"

  echo "createServer $gocd_svm from $from"

  vpcId=$(getDefaultVpcId)
  release="$(getRelease)" 
  imageId="$(getImageId)"
  ecsAuth="$(echo -n "$DOCKER_HUB_USERNAME:$DOCKER_HUB_PASSWORD" | base64)"
  ecsEmail="$DOCKER_HUB_EMAIL"

  fromVolumeIdEtcGo=$(getVolumeIdByPatternAndEnv "etc-go" "$from")
  fromVolumeIdVarLib=$(getVolumeIdByPatternAndEnv "var-lib" "$from")

  if [ -n "$fromVolumeIdEtcGo" ] && [ -n "$fromVolumeIdVarLib" ] ; then
    snapXvdb=$(createSnapshot "$gocd_svm-etc-go"  "$fromVolumeIdEtcGo")
    snapXvdc=$(createSnapshot "$gocd_svm-var-lib" "$fromVolumeIdVarLib")
  fi

  echo "* creating new server stack $gocd_svm"
  aws cloudformation "create-stack" \
  --stack-name "$gocd_svm" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/create-server.cf" \
  --parameters \
    ParameterKey=VpcId,ParameterValue="$vpcId"       \
    ParameterKey=Release,ParameterValue="$release"   \
    ParameterKey=ImageId,ParameterValue="$imageId"   \
    ParameterKey=EcsAuth,ParameterValue="$ecsAuth"   \
    ParameterKey=EcsEmail,ParameterValue="$ecsEmail" \
    ParameterKey=SnapXvdb,ParameterValue="$snapXvdb" \
    ParameterKey=SnapXvdc,ParameterValue="$snapXvdc" \
    ParameterKey=InstanceType,ParameterValue="$instanceType" \
  > /dev/null || return $ERROR_createServer_createStack

  echo "* waiting for $gocd_svm to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$gocd_svm" --output text \
  || return $ERROR_createServer_waitStack

  publicIp="$(getPublicIpAddress "$gocd_svm")" || return $?
  echo "* public IP: $publicIp"

  echo "$gocd_svm"     > "$(dirname "${BASH_SOURCE[0]}")/target/gocdSvm"
  echo "$release"      > "$(dirname "${BASH_SOURCE[0]}")/target/release"
  echo "$snapXvdb"     > "$(dirname "${BASH_SOURCE[0]}")/target/snapXvdb"
  echo "$snapXvdc"     > "$(dirname "${BASH_SOURCE[0]}")/target/snapXvdc"
  echo "$publicIp"     > "$(dirname "${BASH_SOURCE[0]}")/target/publicIp"
  echo "$instanceType" > "$(dirname "${BASH_SOURCE[0]}")/target/instanceType"

  setVolumeNames "$gocd_svm" || return $?
}



function updateServer() {
  local env=$1
  local from=$2

  gocd_svm="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdSvm 2> /dev/null)"

  echo
  echo "updateServer $gocd_svm"

  vpcId=$(getDefaultVpcId)
  imageId="$(getImageId)"
  ecsAuth="$(echo -n "$DOCKER_HUB_USERNAME:$DOCKER_HUB_PASSWORD" | base64)"
  ecsEmail="$DOCKER_HUB_EMAIL"

  release="$(cat  "$(dirname "${BASH_SOURCE[0]}")/target/release"  2> /dev/null)"
  snapXvdb="$(cat "$(dirname "${BASH_SOURCE[0]}")/target/snapXvdb" 2> /dev/null)"
  snapXvdc="$(cat "$(dirname "${BASH_SOURCE[0]}")/target/snapXvdc" 2> /dev/null)"

  instanceType="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/instanceType 2> /dev/null)"

  gocdServerImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdServerImageName 2> /dev/null)"
  gocdAgentImage="$(cat  "$(dirname "${BASH_SOURCE[0]}")"/target/gocdAgentImageName  2> /dev/null)"

  # take a chance to override images
  [ -n "$GOCD_SERVER_IMAGE_TAG" ] && gocdServerImage="$DOCKER_HUB_USERNAME/gocd-server:$GOCD_SERVER_IMAGE_TAG"
  [ -n "$GOCD_AGENT_IMAGE_TAG"  ] &&  gocdAgentImage="$DOCKER_HUB_USERNAME/gocd-agent:$GOCD_AGENT_IMAGE_TAG"

  [ -n "$GOCD_URL" ] && certificateArn="$(getCertificateArn "$GOCD_URL")"

  if [ "$KEEP_GOCD_ADMIN_PASSWORD" != "true" ] ; then 
    gocdAdminOTP="$(openssl rand -base64 12)"
  fi

  "$(dirname "${BASH_SOURCE[0]}")/create_update_template.sh"

  echo "* updating server stack $gocd_svm"
  aws cloudformation "update-stack" \
  --stack-name "$gocd_svm" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/update-server.cf" \
  --parameters \
    ParameterKey=VpcId,ParameterValue="$vpcId"       \
    ParameterKey=Release,ParameterValue="$release"   \
    ParameterKey=ImageId,ParameterValue="$imageId"   \
    ParameterKey=EcsAuth,ParameterValue="$ecsAuth"   \
    ParameterKey=EcsEmail,ParameterValue="$ecsEmail" \
    ParameterKey=SnapXvdb,ParameterValue="$snapXvdb" \
    ParameterKey=SnapXvdc,ParameterValue="$snapXvdc" \
    ParameterKey=InstanceType,ParameterValue="$instanceType"       \
    ParameterKey=GoCDServerImage,ParameterValue="$gocdServerImage" \
    ParameterKey=GoCDAgentImage,ParameterValue="$gocdAgentImage"   \
    ParameterKey=CertificateArn,ParameterValue="$certificateArn"   \
    ParameterKey=GoCDAdminOTP,ParameterValue="$gocdAdminOTP"       \
    ParameterKey=GithubPrivateKey,ParameterValue="$(getGithubPrivateKey)" \
  > /dev/null || return $ERROR_updateServer_updateStack

  echo "* waiting for $gocd_svm to complete. this may take a few minutes."
  aws cloudformation wait stack-update-complete --stack-name "$gocd_svm" --output text \
  || return $ERROR_udpateServer_waitStack
}



function destroyServer() {
  local env=$1
  gocd_svm="$(getStackname "svm" "$env")"
  destroyStack "$gocd_svm" || return $? 
}
