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
ERROR_getVolumeIdByName=19
ERROR_getHostedZoneId=20
ERROR_updateDnsEntry=21
ERROR_getCname=22
ERROR_notAwsHostedZone=23
#ERROR_getResourceTag=24
ERROR_deleteVolume=25
ERROR_getStackResource=26


function switch() {
  local env=$1
  local url=$2

  stack="$(getStackname "svm" "$env")"
  switchVolumes "$stack" "$env"  || return $?
  if [ -n "$url" ] ; then
    switchServer "$stack" "$url" || return $?
  fi
}



function getVolumeIdByName() {
  local name=$1

  aws ec2 describe-volumes \
  --filters Name=tag:Name,Values="$name" \
  --query 'Volumes[0].VolumeId' \
  --output text \
  || return $ERROR_getVolumeIdByName
}



function switchVolumes() {
  local stack=$1
  local env=$2

  etcGo="$(getVolumeIdByPatternAndEnv  "etc-go"  "$env")" || return $?
  varLib="$(getVolumeIdByPatternAndEnv "var-lib" "$env")" || return $?

  if [ -n "$etcGo" ] ; then 
    setResourceTag "$etcGo" "Env" ""  || return $?
  fi

  if [ -n "$varLib" ] ; then 
    setResourceTag "$varLib" "Env" "" || return $?
  fi

  etcGo="$(getVolumeIdByName  "$stack-etc-go")"  || return $?
  varLib="$(getVolumeIdByName "$stack-var-lib")" || return $?

  setResourceTag "$etcGo"  "Env" "$env" || return $?
  setResourceTag "$varLib" "Env" "$env" || return $?
}



function getCname() {
  local stack=$1

  # shellcheck disable=SC2016
  gocdServerUrl=$(
    aws cloudformation describe-stacks \
    --stack-name "$stack" \
    --query 'Stacks[*].Outputs[?OutputKey==`GoCDServerURL`].OutputValue' \
    --output text
  ) || return $ERROR_getCname

  cname="${gocdServerUrl#*://}"
  cname="${cname%%/*}"

  echo "$cname"
}



function getHostedZoneId() {
  local url=$1

  zone="${url#*\.}."

  zoneId=$(
    aws route53 list-hosted-zones-by-name \
    --query 'HostedZones[?Name==`'"$zone"'`].Id' \
    --output text
  ) || return $ERROR_getHostedZoneId

  [ -z "$zoneId" ] \
  && echo "* $zone is not a hosted zone in AWS. not updating. " \
  && return $ERROR_notAwsHostedZone

  zoneId=${zoneId//\/hostedzone\//}

  echo "$zoneId" 
}



function updateDnsRecord() {
  local url=$1
  local cname=$2
  local zoneId=$3

  upsertJson="$(dirname "${BASH_SOURCE[0]}")/target/upsert.json"

  cat <<EOF > "$upsertJson"
{
  "Comment": "Update GoCD Server Record",
  "Changes": [
    {
      "Action": "UPSERT",
      "ResourceRecordSet": {
        "Name": "$url",
        "Type": "CNAME",
        "TTL": 60,
        "ResourceRecords": [ { "Value": "$cname" } ]
      }
    }
  ]
}
EOF

  echo "* updating $url to CNAME $cname"

  aws route53 change-resource-record-sets \
  --hosted-zone-id "$zoneId" \
  --change-batch "file://$upsertJson" \
  || $ERROR_updateDnsEntry
}



function switchServer() {
  local stack=$1
  local url=$2    # alias short name, e.g. gocd.yourcompany.com

  cname=$(getCname "$stack") || return $?        # LB DNS name
  zoneId=$(getHostedZoneId "$url")

  sc=$?

  if [ $sc -ne 0 ] ; then
    if [ $sc -eq $ERROR_notAwsHostedZone ] ; then
      return 0
    else
      return $sc
    fi
  fi
  
  updateDnsRecord "$url" "$cname" "$zoneId" || return $?
}



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
  local stack=$1

  aws ec2 describe-instances \
  --filters \
    Name=tag:Name,Values="$stack" \
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
  local stack=$1

  aws ec2 describe-instances \
  --filters \
    "Name=tag:Name,Values=$stack" \
    "Name=instance-state-name,Values=running" \
  --query "Reservations[*].Instances[*].PublicIpAddress" \
  --output text || return $ERROR_getPublicIpAddress
}



function getCertificateArn() {
  local fqdn=$1

  aws acm list-certificates --query 'CertificateSummaryList[?DomainName==`'"$fqdn"'`].CertificateArn' --output text \
  || return $ERROR_getCertificateArn
}



function getStackResource() {
  local env=$1
  local resource=$2

  stack=$(getStackname "svm" "$env")

  aws cloudformation describe-stack-resources \
  --stack-name "$stack" \
  --query 'StackResources[?LogicalResourceId==`'"$resource"'`].PhysicalResourceId' \
  --output text \
  || return $ERROR_getStackResource
}



function getResourceTag() {
  local resourceId=$1
  local key=$2

  tag=$(
    aws ec2 describe-tags \
    --filters \
      Name=resource-id,Values="$resourceId" \
      Name=Key,Values="$key" \
    --query 'Tags[0].Value' \
    --output text 2> /dev/null
  )

  [ "$tag" != "None" ] && echo "$tag" || echo ""
}



function setResourceTag() {
  local resourceId=$1
  local key=$2
  local value=$3

  aws ec2 create-tags \
  --resources "$resourceId" \
  --tags Key="$key",Value="$value" \
  || return $ERROR_setResourceTag
}



function setVolumeNames() {
  local stack=$1

  instanceId="$(getInstanceId "$stack")"                  || return $?
  volumeIdXvdb="$(getVolumeId "$instanceId" "/dev/xvdb")" || return $?
  volumeIdXvdc="$(getVolumeId "$instanceId" "/dev/xvdc")" || return $?
  setResourceTag "$volumeIdXvdb" "Name" "$stack-etc-go"   || return $?
  setResourceTag "$volumeIdXvdc" "Name" "$stack-var-lib"  || return $?
  setVolumesWithoutName "$instanceId" "$stack"            || return $?
}



function setVolumesWithoutName() {
  local instanceId=$1
  local stack=$2

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
    setResourceTag "$volumeId" "Name" "$stack-$device" || return $?
  done
}



function createServer() {
  local env=$1
  local from=$2
  local instanceType=$3

  [ -z "$DOCKER_HUB_USERNAME" ] && return $ERROR_noDockerHubUsername
  [ -z "$DOCKER_HUB_PASSWORD" ] && return $ERROR_noDockerHubPassword
  [ -z "$DOCKER_HUB_EMAIL" ]    && return $ERROR_noDockerHubEmail

  stack="$(getStackname "svm" "$env")"

  echo "createServer $stack from $from"

  vpcId=$(getDefaultVpcId)
  release="$(getRelease)" 
  imageId="$(getImageId)"
  ecsAuth="$(echo -n "$DOCKER_HUB_USERNAME:$DOCKER_HUB_PASSWORD" | base64)"
  ecsEmail="$DOCKER_HUB_EMAIL"

  fromVolumeIdEtcGo=$(getVolumeIdByPatternAndEnv "etc-go" "$from")
  fromVolumeIdVarLib=$(getVolumeIdByPatternAndEnv "var-lib" "$from")

  if [ -n "$fromVolumeIdEtcGo" ] && [ -n "$fromVolumeIdVarLib" ] ; then
    snapXvdb=$(createSnapshot "$stack-etc-go"  "$fromVolumeIdEtcGo")
    snapXvdc=$(createSnapshot "$stack-var-lib" "$fromVolumeIdVarLib")
  fi

  echo "* creating new server stack $stack"
  aws cloudformation "create-stack" \
  --stack-name "$stack" \
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

  echo "* waiting for $stack to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$stack" --output text \
  || return $ERROR_createServer_waitStack

  publicIp="$(getPublicIpAddress "$stack")" || return $?
  echo "* public IP: $publicIp"

  echo "$stack"        > "$(dirname "${BASH_SOURCE[0]}")/target/gocdSvm"
  echo "$release"      > "$(dirname "${BASH_SOURCE[0]}")/target/release"
  echo "$snapXvdb"     > "$(dirname "${BASH_SOURCE[0]}")/target/snapXvdb"
  echo "$snapXvdc"     > "$(dirname "${BASH_SOURCE[0]}")/target/snapXvdc"
  echo "$publicIp"     > "$(dirname "${BASH_SOURCE[0]}")/target/publicIp"
  echo "$instanceType" > "$(dirname "${BASH_SOURCE[0]}")/target/instanceType"

  setVolumeNames "$stack" || return $?
}



function updateServer() {
  local env=$1
  local from=$2

  stack="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdSvm 2> /dev/null)"

  echo
  echo "updateServer $stack"

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

  echo "* updating server stack $stack"
  aws cloudformation "update-stack" \
  --stack-name "$stack" \
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

  echo "* waiting for $stack to complete. this may take a few minutes."
  aws cloudformation wait stack-update-complete --stack-name "$stack" --output text \
  || return $ERROR_udpateServer_waitStack
}



function deleteVolume() {
  local volumeId=$1

  env=$(getResourceTag "$volumeId" "Env")         || return $?
  if [ "$env" != "live" ] ; then
    echo "* deleting $etcGo"
    aws ec2 delete-volume --volume-id "$volumeId" || return $ERROR_deleteVolume
  else
    echo "* $etcGo marked \"live\", skipping."
  fi
}



# TODO: not unique. relate to attached instance instead of stackname.
function destroyVolumes() {
  local stack=$1

  etcGo=$(getVolumeIdByName  "$stack-etc-go")  || return $?
  [ -n "$etcGo" ]  && deleteVolume "$etcGo"

  varLib=$(getVolumeIdByName "$stack-var-lib") || return $?
  [ -n "$varLib" ] && deleteVolume "$varLib"
}



function destroyServer() {
  local env=$1
  stack="$(getStackname "svm" "$env")"
  destroyStack "$stack"   || return $? 
  destroyVolumes "$stack" || return $?
}
