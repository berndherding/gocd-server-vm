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



function createInstance() {
  local serverStackname=$1

  vpcId=$(getDefaultVpcId)
  imageId=$(getImageId)
  etcGoVolumeId="$(cat "$DIR"/target/etcGoVolumeId)"
  varLibVolumeId="$(cat "$DIR"/target/varLibVolumeId)"

  ecsAuth="$(echo -n "$(getDockerHubUsername):$(getDockerHubPassword)" | base64)"
  ecsEmail="$(getDockerHubEmail)"

  aws cloudformation create-stack \
  --stack-name "$serverStackname" \
  --template-body file://"$DIR/server.cf" \
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
  local clusterStackname=$1
  local serverStackname=$2

  # shellcheck disable=SC2016
  clusterName="$(aws cloudformation describe-stacks \
    --stack-name "$serverStackname" \
    --query 'Stacks[*].Outputs[?OutputKey==`Cluster`].OutputValue' \
    --out text
  )" || return 3

  dockerImage="$(cat "$DIR"/target/gocdServerImageName)"

  aws cloudformation create-stack \
  --stack-name "$clusterStackname" \
  --template-body file://"$DIR/cluster.cf" \
  --parameters \
    ParameterKey=ClusterName,ParameterValue="$clusterName" \
    ParameterKey=DockerImage,ParameterValue="$dockerImage" \
    ParameterKey=GithubPemKey,ParameterValue="$(getGithubPrivateKey)" \
  || return 4

  echo "*** please wait for create-stack to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$clusterStackname" --output text || return 5
}



function createServerInstance() {
  local serverStackname=$1
  local clusterStackname=$2

  createInstance "$serverStackname"                     || return $?
  mountVolumes   "$serverStackname"                     || return $?
  createCluster  "$clusterStackname" "$serverStackname" || return $?
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
  echo "$publicDnsName"    > "$DIR"/target/goServerPublicDnsName
  echo "$publicIpAddress"  > "$DIR"/target/goServerPublicIpAddress
  echo "$privateIpAddress" > "$DIR"/target/goServerPrivateIpAddress
}



function mountVolume() {
  local ip=$1
  local dev=$2
  local mp=$3

  # mount script installed via server.cf
  $SSH "ec2-user@$ip" "sudo /bin/bash /tmp/mountvolume.sh $dev $mp" || return 9
}
