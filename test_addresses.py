#!/usr/bin/env python

import util

base_dir = "/afs/ir/data/saurabh1/pinatrace_out/memcached/%s"

# t1 = util.Trace(base_dir % "2016_04_18_00_17_02_memcached.out")
t1 = util.Trace(base_dir % "latest")
# t2 = util.Trace(base_dir % "2016_04_18_00_17_12_memcached.out")
t2 = util.Trace(base_dir % "latest2")

t1 = t1.aggregate_reads_writes()
t2 = t2.aggregate_reads_writes()

all_page_nos = set(t1.keys() + t2.keys())
unique_page_nos = set([])

for k1 in t1:
  if k1 not in t2:
    print k1, t1[k1]
    unique_page_nos.add(k1)


for i in range(20):
  print
print
print
print
print

for k2 in t2:
  if k2 not in t1:
    print k2, t2[k2]
    unique_page_nos.add(k2)

print "Unique: %d" % len(unique_page_nos)
print "Total: %d" % len(all_page_nos)
