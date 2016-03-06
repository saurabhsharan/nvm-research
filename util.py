import os
import shlex
import subprocess

# 50 GB space
AFS_DIRECTORY = "/afs/ir/data/saurabh1/"

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

def run_under_pin(command_to_run, pin_output_filename):
  pin_path = os.path.join(os.getcwd(), "pin/pin")
  pin_tool_path = os.path.join(os.getcwd(), "pin/source/tools/ManualExamples/obj-intel64/pinatrace.so")
  env_vars = dict(os.environ)
  env_vars["PINATRACE_OUTPUT_FILENAME"] = pin_output_filename
  pin_process = subprocess.Popen([pin_path, "-t", pin_tool_path, "--"] + shlex.split(command_to_run), env=env_vars)
  return pin_process
