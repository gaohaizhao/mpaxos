import boto.ec2
import json
import socket

EC2_ACCESS_ID = 'AKIAJ52SU7UXRK6OVEFA'
EC2_SECRET_KEY = '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'

#AMI_TOKYO = 'ami-7d1d977c'
#AMI_SINGAPORE = 'ami-b02f66e2'
#AMI_SYDNEY = 'ami-934ddea9'
#AMI_IRELAND = 'ami-57b0a223'
#AMI_CALIFORNIA = 'ami-72072e37'

AMI_TOKYO = 'ami-15ea7414'
AMI_SINGAPORE = 'ami-781d572a'
AMI_SYDNEY = 'ami-ff77eac5'
AMI_IRELAND = 'ami-1004e367'
AMI_CALIFORNIA = 'ami-46370303'

KEY_TOKYO = 'mpaxos-tokyo'
KEY_SINGAPORE = 'mpaxos-singapore'
KEY_SYDNEY = 'mpaxos-sydney'
KEY_IRELAND = 'mpaxos-ireland'
KEY_CALIFORNIA = 'mpaxos-california'

AMIS = [AMI_TOKYO, AMI_SINGAPORE, AMI_SYDNEY, AMI_IRELAND, AMI_CALIFORNIA]
KEYS = [KEY_TOKYO, KEY_SINGAPORE, KEY_SYDNEY, KEY_IRELAND, KEY_CALIFORNIA]

INSTANCE_TYPE = 'm1.xlarge'

SECURITY = ['SecurityGroup:default']

conns = [boto.ec2.connect_to_region('ap-northeast-1', aws_access_key_id=EC2_ACCESS_ID, aws_secret_access_key=EC2_SECRET_KEY), # Tokyo
        boto.ec2.connect_to_region('ap-southeast-1', aws_access_key_id=EC2_ACCESS_ID, aws_secret_access_key=EC2_SECRET_KEY), # Singapore
        boto.ec2.connect_to_region('ap-southeast-2', aws_access_key_id=EC2_ACCESS_ID, aws_secret_access_key=EC2_SECRET_KEY), # Sydny
        boto.ec2.connect_to_region('eu-west-1', aws_access_key_id=EC2_ACCESS_ID, aws_secret_access_key=EC2_SECRET_KEY), # Ireland
        boto.ec2.connect_to_region('us-west-1', aws_access_key_id=EC2_ACCESS_ID, aws_secret_access_key=EC2_SECRET_KEY), # California
]
hosts = [""]

def create_instance(conn, ami, key):
    rs = conn.get_all_security_groups()
    print(rs)
    res = conn.run_instances(image_id=ami, key_name=key, instance_type=INSTANCE_TYPE, security_groups=rs)
    print(res)
    print(res.instances)

def show_instance(conn, terminate=False):
    # show instances
    ress = conn.get_all_instances()
    for res in ress:
        instances = res.instances
        for inst in instances:
#        inst = instances[0]
            if inst.state == "terminated":
                continue
            print("instance type:" + inst.instance_type)
            print("instance id:" + inst.id)
            print("instance state:" + inst.state)
            print("instance dns name:" + inst.dns_name)
            ip = socket.gethostbyname(inst.dns_name)
            hosts.append(ip)
            if terminate:
                # terminate instances
                conn.terminate_instances(instance_ids=[inst.id])
    print ""

def terminal_all(conn):
    show_instance(conn, True)

def print_hosts():
    f = open("hosts.sh", "w")
    f.write("N_HOST=5\n")
    f.write("USER=ubuntu\n")
    f.write("MHOST[0]=none\n")
    for i in range(1, 5+1):
        f.write("MHOST[%d]=%s\n" % (i, hosts[i]))

def print_config():
    xxx = {"groups":[1,2,3,4,5,6,7,8,9,10],
           "local_nid":1,
           "local_port":8881,
            "nodes":[
                {
                "nid":1,
                "groups":[1,2,3,4,5,6,7,8,9,10],
                "addr": hosts[1],
                "port":8881
                },
                {
                    "nid":2,
                    "groups":[1,2,3,4,5,6,7,8,9,10],
                    "addr": hosts[2],
                    "port":8882
                },
                {
                    "nid":3,
                    "groups":[1,2,3,4,5,6,7,8,9,10],
                    "addr": hosts[3],
                    "port":8883
                },
                {
                    "nid":4,
                    "groups":[1,2,3,4,5,6,7,8,9,10],
                    "addr": hosts[4],
                    "port":8884
                },
                {
                    "nid":5,
                    "groups":[1,2,3,4,5,6,7,8,9,10],
                    "addr": hosts[5],
                    "port":8885
                }
                ]
            }
    for i in range(1, 5+1):
        f = open("../config/config.5.%d" % i, "w")
        xxx["local_nid"] = i
        xxx["local_port"] = 8880 + i
        f.write(json.dumps(xxx))
        f.close()

def print_jpaxos_config():
    f = open("paxos.properties.remote", "w")
    for i in range(0, 5):
        f.write("process.%d = %s:%d:%d\n" % (i, hosts[i+1], 2000+i, 3000+i))
    f.write("Network = TCP\n")
    f.write("CrashModel = EpochSS\n")

if __name__ == "__main__":
    for conn, key, ami in zip(conns, KEYS, AMIS):
    #    create_instance(conn, ami, key)
        show_instance(conn)

    #    terminal_all(conn)
    print_hosts()
    print_config()
    print_jpaxos_config()
