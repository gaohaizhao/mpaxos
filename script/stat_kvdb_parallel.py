

import re
import sys
import os

import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import matplotlib.font_manager as font_manager
import itertools

OUTPUT = os.path.expanduser("~/Dropbox/paper/rsm/figures/kvdb_parallel.eps")
BASE_DIR = "result.mpaxos.parallel"
node_names = ["BDB", "TK", "SG", "SN", "IL", "CL"]

mpl.rcParams['figure.figsize'] = (8,5)
colors=["0.0", "0.05", "0.10", "0.15", "0.20", "0.25", "0.30", "0.35", "0.40", "0.45", "0.50", "0.55", "0.60", "0.8", "0.85", "0.9", "0.95", "1"]
colors=["0.95", "0.0", "0.2", "0.4", "0.6", "0.8"]

#rates = []
#for j in range(5+1):
#    rates.append([])
#
#for i in range(1, 90+1):
#    for j in range(1, 5+1):
#        rate = None
#        f = open("result.mpaxos.parallel/result.mpaxos.%d.%d" % (i, j))
#        ratesum = 0.0
#        rate=0.0
#        for line in f.readlines():
#            r = re.match(r".+ (\d+) proposals commited in (\d+)ms.+", line)
#            if r:
#                c = int(r.group(1))
#                t = int(r.group(2))
#                rate = c / (t / 1000.0)
#                rates[j].append(rate)
#                ratesum += rate
#        sys.stdout.write("\t%f" % rate)
##    rates[0].append(ratesum)
#    rates[0].append(0)
#    sys.stdout.write("\n")
#
#bottom=np.zeros(len(rates[1]))
#locs = np.arange(len(rates[1]))
#for j in range(1, 5+1):
#    plt.bar(locs, rates[j], bottom=bottom, color=cm.hsv(32*j), label=node_names[j])
#    plt.legend(loc='upper left')
#    bottom += rates[j]

a = 1000.0/ 1.888
s=[a, 861, 729, 851, 610, 772]
print s
for i in range(0, 5+1):
    plt.bar(i, s[i], color=colors[i], edgecolor="black", label=node_names[i], width=0.6)

plt.ylim((0, 1200))
#plt.xticks(np.arange(1), "")
plt.legend(ncol=5)
plt.xlabel("")
plt.ylabel("throughput(op/s)")
plt.savefig(OUTPUT)    
plt.show()

