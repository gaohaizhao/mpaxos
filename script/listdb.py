#!/usr/bin/env python2

import os, sys, bsddb

if len(sys.argv) < 2:
    print 'usage: %s <bdb file>' % sys.argv[0]
    sys.exit(0)

db = bsddb.btopen(sys.argv[1], 'c')

for k in db.keys():
    print '%s -> %s' % (k, db[k])
