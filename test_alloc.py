from intervaltree import *
from util import *
import os
from collections import Counter

t = IntervalTree()

filename = os.path.join("/afs/ir/data/saurabh1/pinatrace_out/memcached", "2016_04_12_04_24_38_memcached_alloc.out")

allocs = parse_memcached_allocs(filename)

pages = {}

functions = Counter()

for alloc in allocs:
  # if alloc[3] == 'try_read_network':
    # continue

  start_address = alloc[0]
  end_address = start_address + alloc[1] - 1

  start_page = start_address / 4096
  end_page = end_address / 4096

  for pageno in range(start_page, end_page+1):
    if pageno not in pages:
      pages[pageno] = set()
    pages[pageno].add(alloc[3])

for pageno in pages:
  if len(pages[pageno]) > 1:
    print pageno, pages[pageno]
  for function_name in pages[pageno]:
    functions[function_name] += 1

print functions.most_common()
