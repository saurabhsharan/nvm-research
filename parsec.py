#!/usr/bin/env python

import os
import subprocess
import sys

# 50 GB space
AFS_DIRECTORY = "/afs/ir/data/saurabh1/"

COMMAND_LINE_ARGS = {
  "blackscholes": "1 %s %s"
}

VALID_INPUT_SIZES = ["test", "simdev", "simsmall", "simmedium", "simlarge", "native"]

class untar_file:
  def __init__(self, tar_filename):
    self.tar_filename = tar_filename

  def __enter__(self):
    self.temp_dir = subprocess.check_output(["mktemp", "-d"]).strip()
    input_filename = subprocess.check_output(["tar", "-xvf", self.tar_filename, "-C", self.temp_dir]).strip()
    assert len(input_filename.split("\n")) == 1
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

def main(app_name, input_size):
  app_dir = "parsec-3.0/pkgs/apps/%s/" % app_name

  with untar_file(app_dir + "inputs/input_%s.tar" % input_size) as input_filename:
    with create_tmp_file() as output_filename:
      full_path_to_app = os.path.join(os.getcwd(), app_dir, "inst/amd64-linux.gcc/bin", app_name)
      command_line_args = COMMAND_LINE_ARGS[app_name] % (input_filename, output_filename)
      print "%s %s" % (full_path_to_app, command_line_args)
      os.system("%s %s" % (full_path_to_app, command_line_args))

      # TODO(saurabh): for some reason subprocess.call() doesn't work here
      # return_code = subprocess.call("%s %s" % (full_path_to_app, command_line_args))
      # print return_code

def print_usage():
  print "Usage: ./parsec.py {app_name} {input_size}"
  print " app_name must be one of:", ", ".join(COMMAND_LINE_ARGS.keys())
  print " input_size must be one of:", ", ".join(VALID_INPUT_SIZES)

if __name__ == "__main__":
  if len(sys.argv) < 3:
    print_usage()
    sys.exit(-1)

  app_name = sys.argv[1]
  input_size = sys.argv[2]

  if app_name not in COMMAND_LINE_ARGS:
    print "Error: Invalid app_name %s" % app_name
    print
    print_usage()
    sys.exit(-1)

  if input_size not in VALID_INPUT_SIZES:
    print "Error: Invalid input_size %s" % input_size
    print
    print_usage()
    sys.exit(-1)

  main(app_name=app_name, input_size=input_size)
