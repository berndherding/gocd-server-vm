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
ERROR_getEnvSnapshotId=10



function getEnvSnapshotId() {
  local environment=$1
  local snapshotname=$2

  aws ec2 describe-snapshots \
  --owner-ids self \
  --filters \
    Name=tag:Name,Values="*$snapshotname*" \
    Name=tag:Env,Values="$environment" \
  --query 'Snapshots[*].SnapshotId' \
  --output text || return $ERROR_getEnvSnapshotId
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



function getCreateParameters() {
  local gocd_vol=$1

}



function createServer() {
  local label=$1
  local from=${2:-label}

  [ -z "$DOCKER_HUB_USERNAME" ] && return $ERROR_noDockerHubUsername
  [ -z "$DOCKER_HUB_PASSWORD" ] && return $ERROR_noDockerHubPassword
  [ -z "$DOCKER_HUB_EMAIL" ]    && return $ERROR_noDockerHubEmail

  gocd_vol="$(getStackname "vol" "$label")"
  gocd_svm="$(getStackname "svm" "$label")"

  echo
  echo "createServer gocd_svm"
  echo "* using volume stack $gocd_vol"

  getRelease > "$(dirname "${BASH_SOURCE[0]}")/target/release"

  imageId="$(getImageId)"      # "ami-e012d48f"
  vpcId=$(getDefaultVpcId)

  release="$(cat "$(dirname "${BASH_SOURCE[0]}")/target/release" 2> /dev/null)"

  ecsAuth="$(echo -n "$DOCKER_HUB_USERNAME:$DOCKER_HUB_PASSWORD" | base64)"
  ecsEmail="$DOCKER_HUB_EMAIL"

  snapXvdb="$(getEnvSnapshotId "$from" "etc-go")"
  snapXvdc="$(getEnvSnapshotId "$from" "var-lib")"

  echo "* creating new server stack $gocd_svm"
  aws cloudformation "create-stack" \
  --stack-name "$gocd_svm" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/create-server.cf" \
  --parameters \
    ParameterKey=Release,ParameterValue="$release"   \
    ParameterKey=VpcId,ParameterValue="$vpcId"       \
    ParameterKey=ImageId,ParameterValue="$imageId"   \
    ParameterKey=EcsAuth,ParameterValue="$ecsAuth"   \
    ParameterKey=EcsEmail,ParameterValue="$ecsEmail" \
    ParameterKey=SnapXvdb,ParameterValue="$snapXvdb" \
    ParameterKey=SnapXvdc,ParameterValue="$snapXvdc" \
  > /dev/null || return $ERROR_createServer_createStack

  echo "* waiting for $gocd_svm to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$gocd_svm" --output text \
  || return $ERROR_createServer_waitStack

  echo "$gocd_vol" > "$(dirname "${BASH_SOURCE[0]}")"/target/gocdVol
  echo "$gocd_svm" > "$(dirname "${BASH_SOURCE[0]}")"/target/gocdSvm

  publicIpAddress="$(getPublicIpAddress "$gocd_svm")" || return $?
  echo "* public IP: $publicIpAddress"
  echo "$publicIpAddress" > "$(dirname "${BASH_SOURCE[0]}")"/target/goServerPublicIpAddress
}



function updateServer() {
  local label=$1
  local from=$2

  gocd_vol="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdVol 2> /dev/null)"
  gocd_svm="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdSvm 2> /dev/null)"

  echo
  echo "updateServer $gocd_svm"
  echo "* using volume stack $gocd_vol"

  imageId="$(getImageId)"      # "ami-e012d48f"
  vpcId=$(getDefaultVpcId)

  release="$(cat "$(dirname "${BASH_SOURCE[0]}")/target/release" 2> /dev/null)"

  ecsAuth="$(echo -n "$DOCKER_HUB_USERNAME:$DOCKER_HUB_PASSWORD" | base64)"
  ecsEmail="$DOCKER_HUB_EMAIL"

  snapXvdb="$(getEnvSnapshotId "$from" "etc-go")"
  snapXvdc="$(getEnvSnapshotId "$from" "var-lib")"

  gocdServerImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdServerImageName 2> /dev/null)"
  gocdAgentImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdAgentImageName   2> /dev/null)"

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
    ParameterKey=Release,ParameterValue="$release"   \
    ParameterKey=VpcId,ParameterValue="$vpcId"       \
    ParameterKey=ImageId,ParameterValue="$imageId"   \
    ParameterKey=EcsAuth,ParameterValue="$ecsAuth"   \
    ParameterKey=EcsEmail,ParameterValue="$ecsEmail" \
    ParameterKey=SnapXvdb,ParameterValue="$snapXvdb" \
    ParameterKey=SnapXvdc,ParameterValue="$snapXvdc" \
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
  local label=$1
  gocd_svm="$(getStackname "svm" "$label")"
  destroyStack "$gocd_svm" || return $? 
}
