#!/usr/bin/env python

import matplotlib
# http://stackoverflow.com/a/3054314
matplotlib.use('Agg')

import json
import matplotlib.mlab as mlab
import matplotlib.pyplot as plt
import os
import sys
import util

def main(app_name):
  path_to_latest_output_file = "/afs/ir/data/saurabh1/pinatrace_out/%s/latest" % app_name

  print "[plot.py] reading data file", path_to_latest_output_file

  if not os.path.exists(path_to_latest_output_file):
    print "ERROR: Could not find latest output file for %s" % app_name
    sys.exit(-1)

  trace = util.Trace(path_to_latest_output_file)

  total_accesses_with_cache = trace.aggregate_reads_writes()
  total_accesses_without_cache = trace.aggregate_reads_writes(with_cache=False)

  # read_write_ratios_with_cache = []
  # for pageno, write_count in pin_data["write_with_cache"].iteritems():
    # read_count = pin_data["read_with_cache"].get(pageno, 0)
    # print pageno, read_count, write_count
    # read_write_ratios_with_cache.append(float(read_count) / float(write_count))

  # for r in read_write_ratios_with_cache:
    # if r > 0.0:
      # print r

  fig = plt.figure()

  axis1 = fig.add_subplot(511)
  axis1.hist(total_accesses_with_cache.values())
  axis1.set_xlabel("# of reads/writes to one page")
  axis1.set_ylabel("# of pages")
  axis1.set_title(app_name + " (with cache)")

  axis2 = fig.add_subplot(513)
  axis2.hist(total_accesses_without_cache.values())
  axis2.set_xlabel("# of reads/writes to one page")
  axis2.set_ylabel("# of pages")
  axis2.set_title(app_name + " (without cache)")

  # axis3 = fig.add_subplot(515)
  # axis3.hist(read_write_ratios_with_cache)
  # axis3.set_xlabel("read:write ratio")
  # axis3.set_ylabel("# of pages")
  # axis3.set_title(app_name + " (with cache)")

  output_graph_image_path = os.path.join(os.path.dirname(path_to_latest_output_file), "page-access-histogram.png")
  fig.savefig(output_graph_image_path)

  print output_graph_image_path

if __name__ == "__main__":
  if len(sys.argv) < 2:
    print "Usage: ./plot.py {app_name}"
    sys.exit(-1)

  app_name = sys.argv[1]

  main(app_name=app_name)
