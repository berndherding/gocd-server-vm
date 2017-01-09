#!/usr/bin/env bash

[ ! -d "$(dirname "${BASH_SOURCE[0]}")/inc"    ] && ln -s ../gocd-base-ami/inc    "$(dirname "${BASH_SOURCE[0]}")/inc"
[ ! -d "$(dirname "${BASH_SOURCE[0]}")/target" ] && ln -s ../gocd-base-ami/target "$(dirname "${BASH_SOURCE[0]}")/target"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/commons.inc"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/keys.inc"



function createInstance() {
  local serverStackname=$1

  vpcId=$(getDefaultVpcId)
  imageId="ami-e012d48f"   # $(getImageId)
  etcGoVolumeId="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/etcGoVolumeId)"
  varLibVolumeId="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/varLibVolumeId)"

  ecsAuth="$(echo -n "$(getDockerHubUsername):$(getDockerHubPassword)" | base64)"
  ecsEmail="$(getDockerHubEmail)"

  aws cloudformation create-stack \
  --stack-name "$serverStackname" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/server.cf" \
  --parameters \
    ParameterKey=VpcId,ParameterValue="$vpcId" \
    ParameterKey=ImageId,ParameterValue="$imageId" \
    ParameterKey=EcsAuth,ParameterValue="$ecsAuth" \
    ParameterKey=EcsEmail,ParameterValue="$ecsEmail" \
    ParameterKey=VolumeIdXvdb,ParameterValue="$etcGoVolumeId" \
    ParameterKey=VolumeIdXvdc,ParameterValue="$varLibVolumeId" \
  || return 1

  echo "*** please wait for create-stack to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$serverStackname" --output text || return 2
}



function createCluster() {
  local serverStackname=$1
  local clusterStackname=$2
  local mapSourceVolumes=$3

  # shellcheck disable=SC2016
  clusterName="$(aws cloudformation describe-stacks \
    --stack-name "$serverStackname" \
    --query 'Stacks[*].Outputs[?OutputKey==`Cluster`].OutputValue' \
    --out text
  )" || return 3

  gocdAgentImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdAgentImageName)"
  gocdServerImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdServerImageName)"
  goServerUrl="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/goServerPublicIpAddress)"

  aws cloudformation create-stack \
  --stack-name "$clusterStackname" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/cluster.cf" \
  --parameters \
    ParameterKey=ClusterName,ParameterValue="$clusterName" \
    ParameterKey=GoCDAgentImage,ParameterValue="$gocdAgentImage" \
    ParameterKey=GoCDServerImage,ParameterValue="$gocdServerImage" \
    ParameterKey=GoServerUrl,ParameterValue="$goServerUrl" \
    ParameterKey=GithubPemKey,ParameterValue="$(getGithubPrivateKey)" \
    ParameterKey=MapSourceVolumes,ParameterValue="$mapSourceVolumes" \
  || return 4

  echo "*** please wait for create-stack to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$clusterStackname" --output text || return 5
}



function createServerInstance() {
  local serverStackname=$1
  local clusterStackname=$2
  local mapSourceVolumes=$3

  createInstance "$serverStackname" || return $?
  mountVolumes   "$serverStackname" || return $?
  createCluster  "$serverStackname" "$clusterStackname" "$mapSourceVolumes" || return $?
}



function destroyServerInstance() {
  local serverStackname=$1
  local clusterStackname=$3

  destroyStack "$clusterStackname" || return $? 

  ip=$(getPublicIpAddress "$serverStackname")

  $SSH "ec2-user@$ip" 'sudo umount /xvdb' || return 6
  $SSH "ec2-user@$ip" 'sudo umount /xvdc' || return 7

  destroyStack "$serverStackname"  || return $?
}



function mountVolumes() {
  local stackname=$1

  publicDnsName=$(getPublicDnsName "$stackname")
  publicIpAddress=$(getPublicIpAddress "$stackname")
  privateIpAddress=$(getPrivateIpAddress "$stackname")

  # mount script installed via server.cf
  mountVolume "$publicIpAddress" /dev/xvdb /xvdb || return $?
  mountVolume "$publicIpAddress" /dev/xvdc /xvdc || return $?

  $SSH ec2-user@"$publicIpAddress" "
    sudo stop ecs
    sudo service docker restart
    sudo start ecs
  "

  # artifacts
  echo "$publicDnsName"    > "$(dirname "${BASH_SOURCE[0]}")"/target/goServerPublicDnsName
  echo "$publicIpAddress"  > "$(dirname "${BASH_SOURCE[0]}")"/target/goServerPublicIpAddress
  echo "$privateIpAddress" > "$(dirname "${BASH_SOURCE[0]}")"/target/goServerPrivateIpAddress
}



function mountVolume() {
  local ip=$1
  local dev=$2
  local mp=$3

  # mount script installed via server.cf
  $SSH "ec2-user@$ip" "sudo /bin/bash /tmp/mountvolume.sh $dev $mp" || return 9
}
