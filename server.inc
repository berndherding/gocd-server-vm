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



function getPublicIpAddress() {
  local stackname=$1

  aws ec2 describe-instances \
  --filters \
    "Name=tag:Name,Values=$stackname" \
    "Name=instance-state-name,Values=running" \
  --query "Reservations[*].Instances[*].PublicIpAddress" \
  --output text || return $ERROR_getPublicIpAddress
}



function createServer() {
  local stackname=$1

  local mapSourceVolumes="map-source-volumes"

  # check if /xvdb is empty. if so, do NOT mapSourceVolumes
  if [ "$(ls -1 "/xvdb")" = "lost+found" ] ; then
    mapSourceVolumes="do-not-map-source-volumes"
  fi

  vpcId=$(getDefaultVpcId)
  imageId="$(getImageId)" # "ami-e012d48f"
  etcGoVolumeId="$(cat  "$(dirname "${BASH_SOURCE[0]}")"/target/etcGoVolumeId)"
  varLibVolumeId="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/varLibVolumeId)"

  ecsAuth="$(echo -n "$(getDockerHubUsername):$(getDockerHubPassword)" | base64)"
  ecsEmail="$(getDockerHubEmail)"

  gocdServerImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdServerImageName 2> /dev/null)"
  gocdAgentImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdAgentImageName 2> /dev/null)"

  # take a chance to override images
  [ -n "$GOCD_SERVER_IMAGE_TAG" ] && gocdServerImage="$(getDockerHubUsername)/gocd-server:$GOCD_SERVER_IMAGE_TAG"
  [ -n "$GOCD_AGENT_IMAGE_TAG"  ] && gocdAgentImage="$(getDockerHubUsername)/gocd-agent:$GOCD_AGENT_IMAGE_TAG"

  aws cloudformation create-stack \
  --stack-name "$stackname" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/server.cf" \
  --parameters \
    ParameterKey=VpcId,ParameterValue="$vpcId" \
    ParameterKey=ImageId,ParameterValue="$imageId" \
    ParameterKey=EcsAuth,ParameterValue="$ecsAuth" \
    ParameterKey=EcsEmail,ParameterValue="$ecsEmail" \
    ParameterKey=VolumeIdXvdb,ParameterValue="$etcGoVolumeId" \
    ParameterKey=VolumeIdXvdc,ParameterValue="$varLibVolumeId" \
    ParameterKey=GoCDServerImage,ParameterValue="$gocdServerImage" \
    ParameterKey=GoCDAgentImage,ParameterValue="$gocdAgentImage" \
    ParameterKey=MapSourceVolumes,ParameterValue="$mapSourceVolumes" \
    ParameterKey=GithubPrivateKey,ParameterValue="$(getGithubPrivateKey)" \
  || return $ERROR_createServer_createStack

  echo "*** please wait for create-stack to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$stackname" --output text \
  || return $ERROR_createServer_waitStack

  getPublicIpAddress "$stackname" > "$(dirname "${BASH_SOURCE[0]}")"/target/goServerPublicIpAddress \
  || return $?
}



function destroyServer() {
  local stackname=$1
  destroyStack "$stackname" || return $? 
}
