import boto.ec2



EC2_ACCESS_ID = 'AKIAJ52SU7UXRK6OVEFA'
EC2_SECRET_KEY = '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'

AMI_TOKYO = 'ami-7d1d977c'
AMI_SINGAPORE = 'ami-b02f66e2'
AMI_SYDNEY = 'ami-934ddea9'
AMI_IRELAND = 'ami-57b0a223'
AMI_CALIFORNIA = 'ami-72072e37'

KEY_TOKYO = 'mpaxos-tokyo'
KEY_SINGAPORE = 'mpaxos-singapore'
KEY_SYDNEY = 'mpaxos-sydney'
KEY_IRELAND = 'mpaxos-ireland'
KEY_CALIFORNIA = 'mpaxos-california'

INSTANCE_TYPE = 'm1.xlarge'

SECURITY = ['SecurityGroup:default']

conn = boto.ec2.connect_to_region('ap-northeast-1', aws_access_key_id=EC2_ACCESS_ID, aws_secret_access_key=EC2_SECRET_KEY) # Tokyo
# conn = boto.ec2.connect_to_region('ap-southeast-1', EC2_ACCESS_ID, EC2_SECRET_KEY) # Singapore
# conn = boto.ec2.connect_to_region('ap-southeast-2', EC2_ACCESS_ID, EC2_SECRET_KEY) # Sydny
# conn = boto.ec2.connect_to_region('eu-west-1', EC2_ACCESS_ID, EC2_SECRET_KEY) # Ireland
# conn = boto.ec2.connect_to_region('us-west-1', EC2_ACCESS_ID, EC2_SECRET_KEY) # California

# create instance
# rs = conn.get_all_security_groups()
# print(rs)
# res = conn.run_instances(image_id=AMI_TOKYO, key_name=KEY_TOKYO, instance_type=INSTANCE_TYPE, security_groups=rs)
# print(res)
# print(res.instances)


# show instances
res = conn.get_all_instances()
instances = res[0].instances
inst = instances[0]
print(inst.instance_type)
print(inst.id)

# terminate instances
conn.terminate_instances(instance_ids=[inst.id])

