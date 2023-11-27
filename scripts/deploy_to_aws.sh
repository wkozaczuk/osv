#!/bin/bash
#

NAME=$1

qemu-img convert -O raw build/release/usr.img build/release/usr.raw
echo "Converted to raw image"

snapshot_id=$(python3 ~/projects/flexible-snapshot-proxy/src/main.py upload build/release/usr.raw | tail -n 1)
echo "Created snapshot: $snapshot_id"

sleep 3

ami_id=$(./scripts/ec2-make-ami.py -n "$NAME" -s "$snapshot_id" | grep '^ami' | tail -n 1)
echo "Created AMI: $ami_id"

cat ~/projects/osv-on-aws/single-ec2/single-instance-parameters.json | sed -e "s/INSTANCE_NAME/$NAME/" | sed -e "s/AMI_ID/$ami_id/" > /tmp/single-instance-parameters.json
aws cloudformation create-stack \
 --stack-name $NAME \
 --template-body file:///home/wkozaczuk/projects/osv-on-aws/single-ec2/single-instance.yaml \
 --capabilities CAPABILITY_IAM \
 --parameters file:///tmp/single-instance-parameters.json
