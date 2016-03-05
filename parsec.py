#!/usr/bin/env python

import os
import subprocess
import sys
import util

COMMAND_LINE_ARGS = {
  "blackscholes": "1 %s %s"
}

VALID_INPUT_SIZES = ["test", "simdev", "simsmall", "simmedium", "simlarge", "native"]

PIN_BASE_DIR_PATH = os.path.join(os.getcwd(), "pin")
PIN_APP_PATH = os.path.join(PIN_BASE_DIR_PATH, "pin")
PIN_TOOL_PATH = os.path.join(PIN_BASE_DIR_PATH, "sources/tools/ManualExamples/obj-intel64/pinatrace.so")

PIN_COMMAND = " ".join([PIN_APP_PATH, "-t", PIN_TOOL_PATH, "--", "%s"])

def full_parsec_command(app_name, input_size):
  if app_name not in COMMAND_LINE_ARGS:
    raise ValueError("Invalid app_name %s, must be one of: %s" % (app_name, ", ".join(COMMAND_LINE_ARGS.keys())))

  if input_size not in VALID_INPUT_SIZES:
    raise ValueError("Invalid input_size %s, must be one of: %s" % (input_size, " ".join(VALID_INPUT_SIZES)))

  app_dir = "parsec-3.0/pkgs/apps/%s/" % app_name

  with util.untar_file(app_dir + "inputs/input_%s.tar" % input_size) as input_filename:
    with util.create_tmp_file() as output_filename:
      full_path_to_app = os.path.join(os.getcwd(), app_dir, "inst/amd64-linux.gcc/bin", app_name)
      command_line_args = COMMAND_LINE_ARGS[app_name] % (input_filename, output_filename)
      return full_path_to_app + " " + command_line_args

def main(app_name, input_size):
  parsec_command = full_parsec_command(app_name, input_size)
  print PIN_COMMAND % parsec_command
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
