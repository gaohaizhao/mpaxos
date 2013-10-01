import re
import sys
import os

import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import matplotlib.font_manager as font_manager
import itertools

OUTPUT = os.path.expanduser("~/Dropbox/paper/rsm/figures/mpaxos_livelock_client.eps")
BASE_DIR = "result.mpaxos.livelock.client"
node_names = ["", "TK", "SG", "SN", "IL", "CL"]

mpl.rcParams['figure.figsize'] = (8,4)

lats = []

# range for n_batch
for i in range(1, 5+1):
    latsum = 0
    for j in range(1, i+1):
        latency = None
        f = open("%s/result.mpaxos.%d.%d" % (BASE_DIR, i, j))
        for line in f.readlines():
            r = re.match(r".+ (\d+) proposals commited in (\d+)ms.+", line)
            if r:
                c = int(r.group(1))
                t = int(r.group(2))
                latency = t / c
                latsum += latency
    sys.stdout.write("\t%f" % (latsum / 5))
    lats.append(latsum / i)
#    rates[0].append(ratesum)
    sys.stdout.write("\n")

#bottom=np.zeros(len(lats[1]))
locs = np.arange(len(lats))
print locs
print lats
#for j in range(1, 1+1):
#    #plt.bar(locs, rates[j], bottom=bottom, color=cm.hsv(32*j), label=node_names[j])
#    plt.plot(rates[j])
#    #plt.legend(loc='upper left')
#    bottom += rates[j]
plt.bar(locs, lats, color="black")

plt.xlabel("number of proposing sites")
plt.ylabel("average latency (ms)")

#xs=[]
#ys=[]
#for i in range(0, 100+1):
#    y = i / 100.0  
#    x = y * y + 3;
#    xs.append(x)
#    ys.append(y)
#plt.plot(xs, ys)
#
#xs=[]
#ys=[]
#for i in range(0, 100+1):
#    y = i / 100.0  
#    x = 2* y * y + 6;
#    xs.append(x)
#    ys.append(y)
#plt.plot(xs, ys)
#
#xs=[]
#ys=[]
#for i in range(0, 100+1):
#    y = i / 100.0  
#    x = 3* y * y + 9;
#    xs.append(x)
#    ys.append(y)
#plt.plot(xs, ys)

plt.savefig(OUTPUT)    
plt.show()
