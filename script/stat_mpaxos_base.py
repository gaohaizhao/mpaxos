
import numpy
import re
import sys
import os

N_HOST = 5
OUTPUT = os.path.expanduser("~/Dropbox/paper/rsm/tables/mpaxos_base.tex")

f = open("result.jpaxos.base/result.jpaxos.0", "r")
for line in f.readlines():
    r = re.match(r"all done\. (\d+) messages committed in (\d+)ms", line);
    if r:
        c = int(r.group(1))
        t = int(r.group(2)) 
        jpaxos = t / c


f = open("result.mpaxos.base/result.mpaxos.5.1.1", "r")
for line in f.readlines():
    r = re.match(r".* (\d+) proposals commited in (\d+)ms.*", line)
    if r:
        c = int(r.group(1))
        t = int(r.group(2))
        mpaxos = t / c
    
f = open (OUTPUT, "w")
f.write("JPaxos & %d \\\\\n" % jpaxos)
f.write("MPaxos & %d \\\\\n" % mpaxos)
print jpaxos
print mpaxos


