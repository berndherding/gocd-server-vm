#!/usr/bin/env bash

[ ! -d "$(dirname "${BASH_SOURCE[0]}")/inc"    ] && ln -s ../gocd-base-ami/inc    "$(dirname "${BASH_SOURCE[0]}")/inc"
[ ! -d "$(dirname "${BASH_SOURCE[0]}")/target" ] && ln -s ../gocd-base-ami/target "$(dirname "${BASH_SOURCE[0]}")/target"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/commons.inc"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/keys.inc"

ERROR_createInstance_createStack=1
ERROR_createInstance_waitStack=2
ERROR_mountVolume_mountVolume=3
ERROR_unmountVolumes_umountXvdb=4
ERROR_unmountVolumes_umountXvdc=5
ERROR_getClusterName_describeStacks=6
ERROR_createCluster_createStack=7
ERROR_createCluster_waitStack=8



function createInstance() {
  local serverStackname=$1

  vpcId=$(getDefaultVpcId)
  imageId="ami-e012d48f"   # $(getImageId)
  etcGoVolumeId="$(cat  "$(dirname "${BASH_SOURCE[0]}")"/target/etcGoVolumeId)"
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
  || return $ERROR_createInstance_createStack

  echo "*** please wait for create-stack to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$serverStackname" --output text \
  || return $ERROR_createInstance_waitStack
}



function destroyInstance() {
  local serverStackname=$1
  destroyStack "$serverStackname" || return $?
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
  $SSH "ec2-user@$ip" "sudo /bin/bash /tmp/mountvolume.sh $dev $mp" \
  || return $ERROR_mountVolume_mountVolume
}




function unmountVolumes() {
  local serverStackname=$1

  ip=$(getPublicIpAddress "$serverStackname")

  $SSH "ec2-user@$ip" 'sudo umount /xvdb' || return $ERROR_unmountVolumes_umountXvdb
  $SSH "ec2-user@$ip" 'sudo umount /xvdc' || return $ERROR_unmountVolumes_umountXvdc
}



function createMachine() {
  local serverStackname=$1

  createInstance "$serverStackname" || return $?
  mountVolumes   "$serverStackname" || return $?
}



function destroyMachine() {
  local serverStackname=$1

  unmountVolumes  "$serverStackname" || return $?
  destroyInstance "$serverStackname" || return $?
}



function getClusterName() {
  local serverStackname=$1
  local clusterName

  # shellcheck disable=SC2016
  clusterName="$(aws cloudformation describe-stacks \
    --stack-name "$serverStackname" \
    --query 'Stacks[*].Outputs[?OutputKey==`Cluster`].OutputValue' \
    --out text
  )" || return $ERROR_getClusterName_describeStacks

  echo "$clusterName"
}



function createCluster() {
  local serverStackname=$1
  local clusterStackname=$2
  local mapSourceVolumes=$3

#
# TODO: mapSourceVolumes should per default be dependent of 
# an existing and populated /etc/go and /var/lib/go-server
#

  clusterName=$(getClusterName "$serverStackname") || return $?

  gocdServerImage="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/gocdServerImageName)"
  goServerUrl="$(cat "$(dirname "${BASH_SOURCE[0]}")"/target/goServerPublicIpAddress)"

  # take a chance to override GoCD Server Image
  [ -n "$GOCD_SERVER_IMAGE" ] && gocdServerImage="$GOCD_SERVER_IMAGE"

  aws cloudformation create-stack \
  --stack-name "$clusterStackname" \
  --template-body file://"$(dirname "${BASH_SOURCE[0]}")/cluster.cf" \
  --parameters \
    ParameterKey=ClusterName,ParameterValue="$clusterName" \
    ParameterKey=GoCDServerImage,ParameterValue="$gocdServerImage" \
    ParameterKey=GoServerUrl,ParameterValue="$goServerUrl" \
    ParameterKey=GithubPrivateKey,ParameterValue="$(getGithubPrivateKey)" \
    ParameterKey=MapSourceVolumes,ParameterValue="$mapSourceVolumes" \
  || return $ERROR_createCluster_createStack

  echo "*** please wait for create-stack to complete. this may take a few minutes."
  aws cloudformation wait stack-create-complete --stack-name "$clusterStackname" --output text \
  || return $ERROR_createCluster_waitStack
}



function destroyCluster() {
  local clusterStackname=$1
  destroyStack "$clusterStackname" || return $? 
}
