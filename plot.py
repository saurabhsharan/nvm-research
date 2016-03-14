#!/usr/bin/env python

import matplotlib
# http://stackoverflow.com/a/3054314
matplotlib.use('Agg')

import json
import matplotlib.mlab as mlab
import matplotlib.pyplot as plt
import os
import sys

def main(app_name):
  path_to_latest_output_file = "/afs/ir/data/saurabh1/pinatrace_out/%s/latest" % app_name

  print "[plot.py] reading data file", path_to_latest_output_file

  if not os.path.exists(path_to_latest_output_file):
    print "ERROR: Could not find latest output file for %s" % app_name
    sys.exit(-1)

  datapoints = []

  total_accesses_with_cache = {}
  total_accesses_without_cache = {}

  with open(path_to_latest_output_file) as f:
    pin_data = json.load(f)

  for pageno, count in pin_data["read_with_cache"].iteritems():
    total_accesses_with_cache[int(pageno)] = total_accesses_with_cache.get(int(pageno), 0) + int(count)
  for pageno, count in pin_data["write_with_cache"].iteritems():
    total_accesses_with_cache[int(pageno)] = total_accesses_with_cache.get(int(pageno), 0) + int(count)

  for pageno, count in pin_data["read_without_cache"].iteritems():
    total_accesses_without_cache[int(pageno)] = total_accesses_without_cache.get(int(pageno), 0) + int(count)
  for pageno, count in pin_data["write_without_cache"].iteritems():
    total_accesses_without_cache[int(pageno)] = total_accesses_without_cache.get(int(pageno), 0) + int(count)


  fig = plt.figure()

  axis1 = fig.add_subplot(311)
  axis1.hist(total_accesses_with_cache.values())
  axis1.set_xlabel("# of reads/writes to one page")
  axis1.set_ylabel("# of pages")
  axis1.set_title(app_name + " (with cache)")

  axis2 = fig.add_subplot(313)
  axis2.hist(total_accesses_without_cache.values())
  axis2.set_xlabel("# of reads/writes to one page")
  axis2.set_ylabel("# of pages")
  axis2.set_title(app_name + " (without cache)")

  output_graph_image_path = os.path.join(os.path.dirname(path_to_latest_output_file), "page-access-histogram.png")
  fig.savefig(output_graph_image_path)

if __name__ == "__main__":
  if len(sys.argv) < 2:
    print "Usage: ./plot.py {app_name}"
    sys.exit(-1)

  app_name = sys.argv[1]

  main(app_name=app_name)
