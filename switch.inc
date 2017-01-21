#!/usr/bin/env bash

[ ! -d "$(dirname "${BASH_SOURCE[0]}")/inc"    ] && ln -s ../gocd-base-ami/inc    "$(dirname "${BASH_SOURCE[0]}")/inc"
[ ! -d "$(dirname "${BASH_SOURCE[0]}")/target" ] && ln -s ../gocd-base-ami/target "$(dirname "${BASH_SOURCE[0]}")/target"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/commons.inc"

# shellcheck source=/dev/null
. "$(dirname "${BASH_SOURCE[0]}")/inc/keys.inc"

ERROR_getVolumeId=1
ERROR_setLabelOnVolume=2
ERROR_getHostedZoneId=3
ERROR_updateDnsEntry=4
ERROR_getCname=5



function switch() {
  local label=$1
  local gocd_url=$2

  gocd_vol="$(getStackname "gocd-vol" "$label")"
  gocd_svm="$(getStackname "gocd-svm" "$label")"

  switchVolume "$gocd_vol" "$label"      || return $?
  if [ -n "$gocd_url" ] ; then
    switchServer "$gocd_svm" "$gocd_url" || return $?
  fi
}



function getVolumeId() {
  local volumestack=$1
  local volumename=$2

  aws ec2 describe-volumes \
  --filters Name=tag:Name,Values="$volumestack-$volumename" \
  --query 'Volumes[0].VolumeId' \
  --output text \
  || return $ERROR_getVolumeId
}



function setLabelOnVolume() {
  local volumeId=$1
  local label=$2

  aws ec2 create-tags \
  --resources "$volumeId" \
  --tags Key=Stage,Value="$label" \
  || return $ERROR_setLabelOnVolume
}



switchVolume() {
  local volumestack=$1
  local label=$2

  volumeIdEtcGo="$(getLabeledVolumeId  "etc-go"  "$label")" || return $?
  volumeIdVarLib="$(getLabeledVolumeId "var-lib" "$label")" || return $?

  if [ -n "$volumeIdEtcGo" ] ; then 
    volumeNameEtcGo="$(getLabeledVolumeName "$volumeIdEtcGo")"
    echo "* deleting \"$label\" label on $volumeNameEtcGo ($volumeIdEtcGo)"
    setLabelOnVolume "$volumeIdEtcGo" ""  || return $?
  fi

  if [ -n "$volumeIdVarLib" ] ; then 
    volumeNameVarLib="$(getLabeledVolumeName "$volumeIdVarLib")"
    echo "* deleting \"$label\" label on $volumeNameVarLib ($volumeIdVarLib)"
    setLabelOnVolume "$volumeIdVarLib" "" || return $?
  fi

  volumeIdEtcGo="$(getVolumeId  "$volumestack" "etc-go")"  || return $?
  volumeIdVarLib="$(getVolumeId "$volumestack" "var-lib")" || return $?

  setLabelOnVolume "$volumeIdEtcGo"  "$label" || return $?
  setLabelOnVolume "$volumeIdVarLib" "$label" || return $?
}



function getHostedZoneId() {
  local hostedZone=$1

  aws route53 list-hosted-zones-by-name \
    --query 'HostedZones[?Name==`'"$hostedZone"'`].Id' \
    --output text \
  || return $ERROR_getHostedZoneId
}



function getCname() {
  local stackname=$1

  # shellcheck disable=SC2016
  aws cloudformation describe-stacks \
  --stack-name "$stackname" \
  --query 'Stacks[*].Outputs[?OutputKey==`DNSName`].OutputValue' \
  --output text \
  || return $ERROR_getCname
}



function switchServer() {
  local stackname=$1
  local gocd_url=$2                  # alias short name, e.g. gocd.yourcompany.com

  cname="$(getCname "$stackname")"   # ELB public dns name

  zone="${gocd_url#*\.}."
  zoneId=$(getHostedZoneId "$zone")

  [ -z "$zoneId" ] && echo "* $zone is not a hosted zone in AWS. not updating. " && return 0

  zoneId=${zoneId//\/hostedzone\//}

  upsertJson="$(dirname "${BASH_SOURCE[0]}")/target/upsert.json"

  cat <<EOF > "$upsertJson"
{
  "Comment": "Update GoCD Server Record",
  "Changes": [
    {
      "Action": "UPSERT",
      "ResourceRecordSet": {
        "Name": "$gocd_url",
        "Type": "A",
        "TTL": 300,
        "ResourceRecords": [ { "Value": "$cname" } ]
      }
    }
  ]
}
EOF

  echo "* updating $gocd_url to CNAME $cname"

  aws route53 change-resource-record-sets \
  --hosted-zone-id "$zoneId" \
  --change-batch "file://$upsertJson" \
  || $ERROR_updateDnsEntry
}
