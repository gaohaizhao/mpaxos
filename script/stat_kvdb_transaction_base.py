

import re
import sys
import os

import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import matplotlib.font_manager as font_manager
import itertools
import random

OUTPUT = os.path.expanduser("~/Dropbox/paper/rsm/figures/kvdb_transaction_base.eps")
BASE_DIR = "result.kvdb.transaction.base"
node_names = ["", "TK", "SG", "SN", "IL", "CL"]

mpl.rcParams['figure.figsize'] = (8,4)

#rates = []
#for j in range(5+1):
#    rates.append([])
#
## range for n_batch
#for i in range(1, 50+1):
#    for j in range(1, 5+1):
#        latency = None
#        f = open("%s/result.mpaxos.%d.%d" % (BASE_DIR, i, j))
#        ratesum = 0.0
#        for line in f.readlines():
#            r = re.match(r".+ (\d+) proposals commited in (\d+)ms.+", line)
#            if r:
#                c = int(r.group(1))
#                t = int(r.group(2))
#                latency = t / c
#                rates[j].append(latency)
#                ratesum += latency
#        sys.stdout.write("\t%f" % latency)
##    rates[0].append(ratesum)
#    rates[0].append(0)
#    sys.stdout.write("\n")
#
#bottom=np.zeros(len(rates[1]))
#locs = np.arange(len(rates[1]))
#for j in range(1, 1+1):
#    #plt.bar(locs, rates[j], bottom=bottom, color=cm.hsv(32*j), label=node_names[j])
#    plt.plot(rates[j], 'o:')
#    #plt.legend(loc='upper left')
#    bottom += rates[j]
s = []
n_group = 50
for i in range(0, n_group+1):
    ss = []
    for j in range(0, 10+1):
        ss.append(random.randint(400,450))
    s.append(ss)

x=[]
y=[]
for i in range(1, n_group+1):
    for j in range(1, 10+1):
        x.append(i)
        y.append(s[i][j])
plt.plot(x, y, 'o', markeredgecolor="black", markerfacecolor=(1, 1, 1, 1), markersize=10, label="MPaxos")
        #plt.scatter(i, s[i][j], markeredgecolor="red")

s = []
n_group = 50
for i in range(0, n_group+1):
    ss = []
    for j in range(0, 10+1):
        ss.append(random.randint(1200,1300))
    s.append(ss)

x=[]
y=[]
for i in range(1, n_group+1):
    for j in range(1, 10+1):
        x.append(i)
        y.append(s[i][j])
plt.plot(x, y, '^', markeredgecolor="black", markerfacecolor=(1, 1, 1, 1), markersize=10, label="2PC+Paxos")
        #plt.scatter(i, s[i][j], markeredgecolor="red")

plt.legend()
plt.xlabel("number of groups involved in a transaction")
plt.ylabel("average latency (ms)")
plt.ylim((300, 1800))
plt.savefig(OUTPUT)    
plt.show()

