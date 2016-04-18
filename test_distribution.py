#!/usr/bin/env python

import os
import util


filenames = \
"""
latest
latest2
""".strip().split("\n")
filenames = [os.path.join("/afs/ir/data/saurabh1/pinatrace_out/memcached/", filename) for filename in filenames]

trace_a = util.Trace(filenames[0])
trace_b = util.Trace(filenames[1])

page_counts_a = trace_a.aggregate_reads_writes().values()
page_counts_b = trace_b.aggregate_reads_writes().values()

print util.compare_page_counts_distributions(page_counts_a, page_counts_b, 10)
