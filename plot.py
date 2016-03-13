#!/usr/bin/env python

import matplotlib
# http://stackoverflow.com/a/3054314
matplotlib.use('Agg')

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

  with open(path_to_latest_output_file) as f:
    for line in f:
      if line.strip() == "## TOTAL":
        break

    for line in f:
      datapoints.append(int(line.strip().split(": ")[1]))

  plt.hist(datapoints)
  plt.xlabel("# of reads/writes to one page")
  plt.ylabel("# of pages")
  plt.title(app_name)

  output_graph_image_path = os.path.join(os.path.dirname(path_to_latest_output_file), "page-access-histogram.png")
  plt.savefig(output_graph_image_path)

if __name__ == "__main__":
  if len(sys.argv) < 2:
    print "Usage: ./plot.py {app_name}"
    sys.exit(-1)

  app_name = sys.argv[1]

  main(app_name=app_name)
