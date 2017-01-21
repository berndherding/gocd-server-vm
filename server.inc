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
  local stackname=$1

  echo
  echo "createServer for $stackname"

  vpcId=$(getDefaultVpcId)
  imageId="$(getImageId)" # "ami-e012d48f"

  ecsAuth="$(echo -n "$(getDockerHubUsername):$(getDockerHubPassword)" | base64)"
  ecsEmail="$(getDockerHubEmail)"

  volumeStack="${stackname//svm/vol}"
  echo "* using $volumeStack associated volume stack"

  gocdServerImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdServerImageName 2> /dev/null)"
  gocdAgentImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdAgentImageName   2> /dev/null)"

  # take a chance to override images
  [ -n "$GOCD_SERVER_IMAGE_TAG" ] && gocdServerImage="$(getDockerHubUsername)/gocd-server:$GOCD_SERVER_IMAGE_TAG"
  [ -n "$GOCD_AGENT_IMAGE_TAG"  ] && gocdAgentImage="$(getDockerHubUsername)/gocd-agent:$GOCD_AGENT_IMAGE_TAG"

  echo "* GoCD server image: $gocdServerImage"
  echo "* GoCD agent image:  $gocdAgentImage"

  [ -n "$GOCD_URL" ] && certificateArn="$(getCertificateArn "$GOCD_URL")"

  if [ "$KEEP_GOCD_ADMIN_PASSWORD" != "true" ] ; then 
    echo "* creating GoCD Admin OTP"
    gocdAdminOTP="$(openssl rand -base64 12)"
  fi

  echo "* creating new server stack $stackname"
  aws cloudformation create-stack \
  --stack-name "$stackname" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/server.cf" \
  --parameters \
    ParameterKey=VpcId,ParameterValue="$vpcId" \
    ParameterKey=ImageId,ParameterValue="$imageId" \
    ParameterKey=EcsAuth,ParameterValue="$ecsAuth" \
    ParameterKey=EcsEmail,ParameterValue="$ecsEmail" \
    ParameterKey=VolumeStack,ParameterValue="$volumeStack" \
    ParameterKey=GoCDServerImage,ParameterValue="$gocdServerImage" \
    ParameterKey=GoCDAgentImage,ParameterValue="$gocdAgentImage" \
    ParameterKey=GithubPrivateKey,ParameterValue="$(getGithubPrivateKey)" \
    ParameterKey=CertificateArn,ParameterValue="$certificateArn" \
    ParameterKey=GoCDAdminOTP,ParameterValue="$gocdAdminOTP" \
  > /dev/null || return $ERROR_createServer_createStack

  echo "* waiting for $stackname to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$stackname" --output text \
  || return $ERROR_createServer_waitStack

  publicIpAddress="$(getPublicIpAddress "$stackname")" || return $?
  echo "* public IP: $publicIpAddress"
  echo "$publicIpAddress" > "$(dirname "${BASH_SOURCE[0]}")"/target/goServerPublicIpAddress
}



function destroyServer() {
  local stackname=$1
  destroyStack "$stackname" || return $? 
}
