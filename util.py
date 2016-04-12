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

  def aggregate_reads_writes(self, with_cache=True):
    if with_cache:
      data_to_aggregate = self.trace_data["cache"]
    else:
      data_to_aggregate = self.trace_data["no_cache"]

    return self._combine_dicts([td['reads'] for td in data_to_aggregate] + [td['writes'] for td in data_to_aggregate])

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

def run_under_pin(command_to_run, pin_output_filename, child_injection=False):
  pin_path = os.path.join(RESEARCH_DIR, "pin/pin")
  pin_tool_path = os.path.join(RESEARCH_DIR, "pin/source/tools/ManualExamples/obj-intel64/pinatrace.so")
  env_vars = dict(os.environ)
  env_vars["PINATRACE_OUTPUT_FILENAME"] = pin_output_filename
  if child_injection:
    injection_method = "child"
  else:
    injection_method = "dynamic"
  pin_process = subprocess.Popen([pin_path, "-injection", injection_method, "-t", pin_tool_path, "--"] + shlex.split(command_to_run), env=env_vars)
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
