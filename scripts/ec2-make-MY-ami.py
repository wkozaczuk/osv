#!/usr/bin/python2

import boto3, argparse, urllib, time, json, subprocess, os.path
import argparse

class Metadata(object):
    base = 'http://169.254.169.254/latest/meta-data/'
    def _get(self, what):
        return urllib.urlopen(Metadata.base + what).read()
    def instance_id(self):
        return self._get('instance-id')
    def availability_zone(self):
        return self._get('placement/availability-zone')
    def region(self):
        return self.availability_zone()[:-1]

def wait_for(ec2_obj, status='available'):
    while ec2_obj.state != status:
        #print('object status {} wanted {}'.format(ec2_obj.status, status))
        time.sleep(1)
        ec2_obj.reload()

#input = '/home/ubuntu/capstan-java-example.img'
input = '/home/ubuntu/small-osv-node.img'

def to_gib(size):
    gib = 1 << 30
    return (size + gib - 1) >> 30

def image_size(filename):
    info = json.loads(subprocess.check_output(['qemu-img', 'info', '--output=json', filename]))
    return info['virtual-size']

def copy_image(img, out):
    subprocess.check_call(['sudo', 'cp', img, out])

def make_ami(input, name):
    metadata = Metadata()
    print('Connecting')
    conn = boto3.resource('ec2',region_name="us-west-2")
    #conn = ec2.connect_to_region(metadata.region())
    print('Creating volume')
    vol = conn.create_volume(Size=to_gib(image_size(input)),
                              AvailabilityZone=metadata.availability_zone(),
                              )
    print('Waiting for {}'.format(vol.id))
    wait_for(vol)
    #vol = conn.describe_volumes([vol.id])[0]
    print('Attaching {} to {}'.format(vol.id, metadata.instance_id()))
    att = vol.attach_to_instance(InstanceId=metadata.instance_id(), Device='xvdf')
    while not os.path.exists('/dev/xvdf'):
        #print('waiting for volume to attach')
        time.sleep(1)
    print('Copying image')
    copy_image(input, '/dev/xvdf')
    print('Detaching {}'.format(vol.id))
    vol.detach_from_instance()
    print('Creating snapshot from {}'.format(vol.id))
    snap = vol.create_snapshot()
    #snap = conn.get_all_snapshots([snap.id])[0]
    wait_for(snap, 'completed')
    print('Deleting {}'.format(vol.id))
    vol.delete()
    print('Registering image from {}'.format(snap.id))
    ami = conn.register_image(Name=name,
                              Architecture='x86_64',
                              RootDeviceName='xvda',
                              VirtualizationType='hvm',
                              BlockDeviceMappings=[
                                 {           
            				'Ebs': {
 	   			        'SnapshotId': snap.id,
                			'VolumeSize': 123,
 			                'DeleteOnTermination': True
                                        }
        			},
			    ])
    print('ami {} created\n'.format(ami))
    return ami

if __name__ == "__main__":
    # Parse arguments
    parser = argparse.ArgumentParser(prog='run')
    parser.add_argument("-n", "--name", action="store", default="test-ami",
                        help="ami name to be created")

    args = parser.parse_args()
    make_ami(input, args.name)
