from util import *

LOG_DIR = "/afs/ir/data/saurabh1/pinatrace_out/memcached/"

runs = convert_memcached_logs_to_dict(["memcached_log3.txt", "memcached_log4.txt", "memcached_log5.txt"])

for (value_size, records) in runs:
  try:
    filenames = runs[(value_size, records)]

    if len(filenames) < 2:
      continue

    print value_size, "*", records, "=", sizeof_fmt(value_size * records)

    for (filename, time_sec) in filenames:
      print time_sec
      print page_counts_distributions(Trace(os.path.join(LOG_DIR, filename)).aggregate_reads_writes().values(), bucket_size=10).most_common()
      print

    print
    print
    print
    print
  except:
    print "ERROR!! Ignoring..."
    print
    print

    continue
