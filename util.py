from collections import Counter
import json
import math
import os
import shlex
import subprocess

RESEARCH_DIR = "/afs/ir/users/s/a/saurabh1/research"

# 50 GB space
AFS_DIRECTORY = "/afs/ir/data/saurabh1/"

class Trace:
  def __init__(self, trace_filename):
    # read entire trace once in constructor
    with open(trace_filename) as f:
      self.trace_data = json.load(f)

    # stay backwards-compatible with old trace files
    if 'header' in self.trace_data:
      self.header = self.trace_data['header']
    if 'data' in self.trace_data:
      self.trace_data = self.trace_data['data']

  def _combine_dicts(self, dicts):
    result = {}

    for d in dicts:
      for k in d:
        pageno = int(k)
        count = int(d[k])
        result[pageno] = count + result.get(pageno, 0)

    return result

  def aggregate_writes(self, with_cache=True):
    if "cache" in self.trace_data and "no_cache" in self.trace_data:
      if with_cache:
        data_to_aggregate = self.trace_data["cache"]
      else:
        data_to_aggregate = self.trace_data["no_cache"]

      return self._combine_dicts([td['writes'] for td in data_to_aggregate])

    elif "read_with_cache" in self.trace_data and "read_without_cache" in self.trace_data and "write_with_cache" in self.trace_data and "write_without_cache" in self.trace_data:
      if with_cache:
        return self._combine_dicts([self.trace_data["write_with_cache"]])
      else:
        return self._combine_dicts([self.trace_data["write_without_cache"]])

  def aggregate_reads(self, with_cache=True):
    if "cache" in self.trace_data and "no_cache" in self.trace_data:
      if with_cache:
        data_to_aggregate = self.trace_data["cache"]
      else:
        data_to_aggregate = self.trace_data["no_cache"]

      return self._combine_dicts([td['reads'] for td in data_to_aggregate])

    elif "read_with_cache" in self.trace_data and "read_without_cache" in self.trace_data and "write_with_cache" in self.trace_data and "write_without_cache" in self.trace_data:
      if with_cache:
        return self._combine_dicts([self.trace_data["read_with_cache"]])
      else:
        return self._combine_dicts([self.trace_data["read_without_cache"]])

  def aggregate_reads_writes(self, with_cache=True):
    if "cache" in self.trace_data and "no_cache" in self.trace_data:
      if with_cache:
        data_to_aggregate = self.trace_data["cache"]
      else:
        data_to_aggregate = self.trace_data["no_cache"]

      return self._combine_dicts([td['reads'] for td in data_to_aggregate] + [td['writes'] for td in data_to_aggregate])

    elif "read_with_cache" in self.trace_data and "read_without_cache" in self.trace_data and "write_with_cache" in self.trace_data and "write_without_cache" in self.trace_data:
      if with_cache:
        return self._combine_dicts([self.trace_data["read_with_cache"], self.trace_data["write_with_cache"]])
      else:
        return self._combine_dicts([self.trace_data["read_without_cache"], self.trace_data["write_without_cache"]])

class untar_file:
  def __init__(self, tar_filename):
    self.tar_filename = tar_filename

  def __enter__(self):
    self.temp_dir = subprocess.check_output(["mktemp", "-d"]).strip()
    input_filename = subprocess.check_output(["tar", "-xvf", self.tar_filename, "-C", self.temp_dir]).strip().split("\n")[0]
    # assert len(input_filename.split("\n")) == 1
    return os.path.join(self.temp_dir, input_filename)

  def __exit__(self, exception_type, exception_value, traceback):
    # TODO(saurabh): delete self.temp_dir
    pass

class create_tmp_file:
  def __enter__(self):
    return os.path.join(AFS_DIRECTORY, "parsec.out")

  def __exit__(self, exception_type, exception_value, traceback):
    # TODO(saurabh): delete parsec.out
    pass

def run_under_pin(command_to_run, pin_output_filename, child_injection=False, memcached_alloc_filename=""):
  pin_path = os.path.join(RESEARCH_DIR, "pin/pin")
  pin_tool_path = os.path.join(RESEARCH_DIR, "pin/source/tools/ManualExamples/obj-intel64/pinatrace.so")
  env_vars = dict(os.environ)
  env_vars["PINATRACE_OUTPUT_FILENAME"] = pin_output_filename
  env_vars["MEMCACHED_ALLOC_FILENAME"] = memcached_alloc_filename
  if child_injection:
    injection_method = "child"
  else:
    injection_method = "dynamic"
  disable_aslr_command = "setarch x86_64 -R"
  pin_process = subprocess.Popen(shlex.split(disable_aslr_command) + [pin_path, "-injection", injection_method, "-t", pin_tool_path, "--"] + shlex.split(command_to_run), env=env_vars)
  return pin_process

# Uses https://en.wikipedia.org/wiki/Percentile#The_Nearest_Rank_method
def percentile_slice(elems, percentile):
  assert 0 <= percentile <= 100
  n = int(math.ceil((percentile / 100.0) * len(elems)))
  assert 1 <= n <= len(elems)
  return elems[:n]

def write_header_to_json_data_file(header, filename):
  with open(filename, 'r') as f:
    data = json.load(f)

  result = {}
  result['header'] = header
  result['data'] = data

  with open(filename, 'w+') as f:
    f.write(json.dumps(result))

class PgrepError(Exception):
  pass

def get_children_pids(parent_pid):
  pgrep_command = "pgrep -P %r" % parent_pid
  pgrep_process = subprocess.Popen(shlex.split(pgrep_command), stdout=subprocess.PIPE)
  pgrep_process.wait()

  if pgrep_process.returncode != 0:
    raise PgrepError()

  children_pids = pgrep_process.stdout.read().strip().split("\n")
  return children_pids

def page_counts_distributions(page_counts, bucket_size = 5):
  buckets = Counter()

  for page_count in page_counts:
    buckets[page_count / bucket_size] += 1

  return buckets

def cosine_similarity(v1,v2):
  sumxx, sumxy, sumyy = 0, 0, 0
  for i in range(len(v1)):
    x = v1[i]; y = v2[i]
    sumxx += x*x
    sumyy += y*y
    sumxy += x*y
  return sumxy/math.sqrt(sumxx*sumyy)

def find_alloc_files(memcached_log_filename, fb_dist=False):
  result = []

  with open(memcached_log_filename, 'rb') as f:
    data = f.read().strip().split("\n\n")

  for run in data:
    if (fb_dist and "fb_ia" not in run) or (not fb_dist and "fb_ia" in run):
      continue

    args, out_filename, _ = run.strip().split("\n")

    args = args.split(" ")

    assert "valuesize" in args[-3]
    assert "records" in args[-2]
    assert "time" in args[-1]

    value_size = int(args[-3].split("=")[1])
    records = int(args[-2].split("=")[1])
    time_sec = int(args[-1].split("=")[1])

    out_filename = out_filename[1:]

    alloc_filename = out_filename.strip().split(".")[0] + "_alloc.out"
    result.append((out_filename, alloc_filename))

  return result



def convert_memcached_logs_to_dict(memcached_log_filenames, fb_dist=False):
  result = {}

  for memcached_log_filename in memcached_log_filenames:
    with open(memcached_log_filename, 'rb') as f:
      data = f.read().strip().split("\n\n")

    for run in data:
      if (fb_dist and "fb_ia" not in run) or (not fb_dist and "fb_ia" in run):
        continue

      args, out_filename, _ = run.strip().split("\n")

      args = args.split(" ")

      assert "valuesize" in args[-3]
      assert "records" in args[-2]
      assert "time" in args[-1]

      value_size = int(args[-3].split("=")[1])
      records = int(args[-2].split("=")[1])
      time_sec = int(args[-1].split("=")[1])

      out_filename = out_filename[1:]

      result_key = (value_size, records)

      if result_key not in result:
        result[result_key] = []

      result[result_key].append((out_filename, time_sec))

  return result


def sizeof_fmt(num, suffix='B'):
  for unit in ['','Ki','Mi','Gi','Ti','Pi','Ei','Zi']:
    if abs(num) < 1024.0:
      return "%3.1f%s%s" % (num, unit, suffix)
    num /= 1024.0
  return "%.1f%s%s" % (num, 'Yi', suffix)

def parse_memcached_allocs(memcached_alloc_filename):
  result = []

  with open(memcached_alloc_filename) as f:
    data = f.read().strip().split("\n")

  for line in data:
    if "alloc" not in line:
      continue

    comps = line.split(" ")
    size = int(comps[0][6:-1])
    address = int(comps[-1], base=16)
    c_filename, function_name, line_number = comps[2].strip().split(":")
    result.append((address, size, c_filename, function_name, line_number))

  return result
