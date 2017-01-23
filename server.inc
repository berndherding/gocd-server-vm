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

  aws acm list-certificates \
  | jq -r '.CertificateSummaryList[] | select(.DomainName=="'"$fqdn"'").CertificateArn' \
  || return $ERROR_getCertificateArn
}



function createServer() {
  local label=$1
  local update=$2

  [ -z "$DOCKER_HUB_USERNAME" ] && return $ERROR_noDockerHubUsername
  [ -z "$DOCKER_HUB_PASSWORD" ] && return $ERROR_noDockerHubPassword
  [ -z "$DOCKER_HUB_EMAIL" ]    && return $ERROR_noDockerHubEmail

  gocd_vol="$(getStackname "vol" "$label")"
  gocd_svm="$(getStackname "svm" "$label")"

  echo
  echo "createServer for $gocd_svm"

  release=$(getRelease)
  vpcId=$(getDefaultVpcId)
  imageId="$(getImageId)" # "ami-e012d48f"

  ecsAuth="$(echo -n "$DOCKER_HUB_USERNAME:$DOCKER_HUB_PASSWORD" | base64)"
  ecsEmail="$DOCKER_HUB_EMAIL"

  echo "* using associated volume stack $gocd_vol"

  gocdServerImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdServerImageName 2> /dev/null)"
  gocdAgentImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdAgentImageName   2> /dev/null)"

  # take a chance to override images
  [ -n "$GOCD_SERVER_IMAGE_TAG" ] && gocdServerImage="$DOCKER_HUB_USERNAME/gocd-server:$GOCD_SERVER_IMAGE_TAG"
  [ -n "$GOCD_AGENT_IMAGE_TAG"  ] &&  gocdAgentImage="$DOCKER_HUB_USERNAME/gocd-agent:$GOCD_AGENT_IMAGE_TAG"

  echo "* GoCD server image: $gocdServerImage"
  echo "* GoCD agent image:  $gocdAgentImage"

  [ -n "$GOCD_URL" ] && certificateArn="$(getCertificateArn "$GOCD_URL")"

  if [ "$KEEP_GOCD_ADMIN_PASSWORD" != "true" ] ; then 
    echo "* creating GoCD Admin OTP"
    gocdAdminOTP="$(openssl rand -base64 12)"
  fi

  stacks="$(aws cloudformation describe-stacks \
    --stack-name "$gocd_svm" \
    --query 'Stacks[*].StackId' \
    --output text 2> /dev/null \
  )"

  if [ "$update" = "update-stack" ] && [ -n "$stacks" ] ; then
    command="update-stack"
    wait="stack-update-complete"
    template="update-server.cf"
  else
    command="create-stack"
    wait="stack-create-complete"
    template="create-server.cf"
  fi

  echo "* creating new server stack $gocd_svm"
  aws cloudformation "$command" \
  --stack-name "$gocd_svm" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/$template" \
  --parameters \
    ParameterKey=Release,ParameterValue="$release" \
    ParameterKey=VpcId,ParameterValue="$vpcId" \
    ParameterKey=ImageId,ParameterValue="$imageId" \
    ParameterKey=EcsAuth,ParameterValue="$ecsAuth" \
    ParameterKey=EcsEmail,ParameterValue="$ecsEmail" \
    ParameterKey=VolumeStack,ParameterValue="$gocd_vol" \
    ParameterKey=GoCDServerImage,ParameterValue="$gocdServerImage" \
    ParameterKey=GoCDAgentImage,ParameterValue="$gocdAgentImage" \
    ParameterKey=GithubPrivateKey,ParameterValue="$(getGithubPrivateKey)" \
    ParameterKey=CertificateArn,ParameterValue="$certificateArn" \
    ParameterKey=GoCDAdminOTP,ParameterValue="$gocdAdminOTP" \
  > /dev/null || return $ERROR_createServer_createStack

  echo "* waiting for $gocd_svm to complete. this may take a few minutes."
  aws cloudformation wait "$wait" --stack-name "$gocd_svm" --output text \
  || return $ERROR_createServer_waitStack

  publicIpAddress="$(getPublicIpAddress "$gocd_svm")" || return $?
  echo "* public IP: $publicIpAddress"
  echo "$publicIpAddress" > "$(dirname "${BASH_SOURCE[0]}")"/target/goServerPublicIpAddress
}



function destroyServer() {
  local label=$1
  gocd_svm="$(getStackname "svm" "$label")"
  destroyStack "$gocd_svm" || return $? 
}
