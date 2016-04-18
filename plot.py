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
  # path_to_latest_output_file = "/afs/ir/data/saurabh1/pinatrace_out/%s/latest" % app_name
  path_to_latest_output_file = "/afs/ir/data/saurabh1/pinatrace_out/memcached/%s" % app_name

  print "[plot.py] reading data file", path_to_latest_output_file

  if not os.path.exists(path_to_latest_output_file):
    print "ERROR: Could not find latest output file for %s" % app_name
    sys.exit(-1)

  trace = util.Trace(path_to_latest_output_file)

  # `all_memory_accesses` is list of (pageno, count) pairs sorted by count
  all_memory_accesses = list(trace.aggregate_writes(with_cache=False).iteritems())
  all_memory_accesses = sorted(all_memory_accesses, key=lambda x: x[1], reverse=True)

  def num_total_accesses(memory_accesses):
    return sum(ma[1] for ma in memory_accesses)

  total_num_memory_accesses = num_total_accesses(all_memory_accesses)

  for i in range(1, 11):
    percentile = i * 10
    subset_memory_accesses = util.percentile_slice(all_memory_accesses, percentile)
    count = num_total_accesses(subset_memory_accesses)
    print "%d percentile: %f percent (%d / %d)" % (i * 10, (float(count) / float(total_num_memory_accesses)) * 100, count, total_num_memory_accesses)

  top_10_list = all_memory_accesses[:10]
  print top_10_list
  print "Top 10: %f" % ((float(num_total_accesses(top_10_list)) / float(total_num_memory_accesses)))

  # print all_memory_accesses
  # print total_num_memory_accesses

  print
  print "Accessed %d total pages" % len(all_memory_accesses)

  return

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

  output_graph_image_path = os.path.join(os.path.dirname(path_to_latest_output_file), "page-access-histogram.png")
  fig.savefig(output_graph_image_path)

  print output_graph_image_path

if __name__ == "__main__":
  if len(sys.argv) < 2:
    print "Usage: ./plot.py {app_name}"
    sys.exit(-1)

  app_name = sys.argv[1]

  main(app_name=app_name)
