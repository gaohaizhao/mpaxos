

import re
import sys
import os

import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import matplotlib.font_manager as font_manager
import itertools

OUTPUT = os.path.expanduser("~/Dropbox/paper/rsm/figures/mpaxos_transaction_base.eps")
BASE_DIR = "result.mpaxos.transaction.base"
node_names = ["", "TK", "SG", "SN", "IL", "CL"]
colors=["0.0", "0.05", "0.10", "0.15", "0.20", "0.25", "0.30", "0.35", "0.40", "0.45", "0.50", "0.55", "0.60", "0.8", "0.85", "0.9", "0.95", "1"]
colors=["0.0", "0.2", "0.4", "0.6", "0.8", "1.0"]

mpl.rcParams['figure.figsize'] = (8,4)

lats = []
for j in range(5+1):
    lats.append([])

loop_batch = [5, 10, 15, 20, 25, 30, 35, 40]
# range for n_batch
for i in loop_batch:
    for j in range(1, 5+1):
        latency = None
        f = open("%s/result.mpaxos.%d.%d" % (BASE_DIR, i, j))
        latsum = 0.0
        for line in f.readlines():
            r = re.match(r".+ (\d+) proposals commited in (\d+)ms.+", line)
            if r:
                c = int(r.group(1))
                t = int(r.group(2))
                latency = t / c
                lats[j].append(latency)
        sys.stdout.write("\t%f" % latency)
#    rates[0].append(ratesum)
    lats[0].append(0)
    sys.stdout.write("\n")



#for j in range(1, 1+1):
#    #plt.bar(locs, rates[j], bottom=bottom, color=cm.hsv(32*j), label=node_names[j])
#    plt.plot(rates[j])
#    #plt.legend(loc='upper left')
#    bottom += rates[j]
s = [[390, 402, 420, 422], 
[393, 400, 410, 417], 
[385, 390, 400, 410], 
[401, 402, 405, 412], 
[398, 402, 411, 422]]
width = 0.15
locs = np.arange(len(lats[1]))
for i in range(1, 5+1):
    plt.bar(locs+i*width, lats[i], width=width, label=node_names[i], color=colors[i]) 

plt.xticks(np.arange(0.4, len(loop_batch), 1), loop_batch)
plt.legend(ncol=5)
plt.ylim((0, 800))
plt.xlabel("number of groups involved in a transaction")
plt.ylabel("latency (ms)")
plt.savefig(OUTPUT)    
plt.show()

