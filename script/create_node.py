from libcloud.compute.types import Provider
from libcloud.compute.providers import get_driver

import libcloud.security

libcloud.security.VERIFY_SSL_CERT = False


EC2_ACCESS_ID = 'AKIAJ52SU7UXRK6OVEFA'
EC2_SECRET_KEY = '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'

Driver = get_driver(Provider.EC2)
conn = Driver(EC2_ACCESS_ID, EC2_SECRET_KEY)

nodes = conn.list_nodes()
print(nodes)