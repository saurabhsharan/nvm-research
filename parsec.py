#!/usr/bin/env python

import os
import subprocess
import sys
import util

COMMAND_LINE_ARGS = {
  "blackscholes": "1 %(input_file)s %(output_file)s",
  "bodytrack": "%(input_file)s 4 1 5 1 0 1",
  # TODO(saurabh): look into facesim
  # "facesim": ""
  "ferret": "%(input_file)s 5 5 1 %(output_file)s",
  "fluidanimate": "1 1 %s %s",
  "freqmine": "%(input_file)s 1",
  "raytrace": "%(input_file)s -automove -nthreads 1 -frames 1 -res 1 1",
}

VALID_INPUT_SIZES = ["test", "simdev", "simsmall", "simmedium", "simlarge", "native"]

PIN_BASE_DIR_PATH = os.path.join(os.getcwd(), "pin")
PIN_APP_PATH = os.path.join(PIN_BASE_DIR_PATH, "pin")
PIN_TOOL_PATH = os.path.join(PIN_BASE_DIR_PATH, "source/tools/ManualExamples/obj-intel64/pinatrace.so")

PIN_COMMAND = " ".join([PIN_APP_PATH, "-t", PIN_TOOL_PATH, "--", "%s"])

def main(app_name, input_size):
  app_dir = os.path.join(os.getcwd(), "parsec-3.0/pkgs/apps/%s/" % app_name)

  with util.untar_file(os.path.join(app_dir, "inputs/input_%s.tar" % input_size)) as input_filename:
    with util.create_tmp_file() as output_filename:
      if app_name == "raytrace":
        full_path_to_app = os.path.join(app_dir, "inst/amd64-linux.gcc/bin", "rtview")
      else:
        full_path_to_app = os.path.join(app_dir, "inst/amd64-linux.gcc/bin", app_name)

      if app_name == "ferret":
        queries_path = os.path.join(os.path.dirname(input_filename.rstrip("/")), "queries")
        input_filename = " ".join([input_filename, "lsh", queries_path])

      command_line_args = COMMAND_LINE_ARGS[app_name] % dict(input_file=input_filename, output_file=output_filename)
      parsec_command = full_path_to_app + " " + command_line_args
      env_vars = dict(os.environ)
      print parsec_command
      subprocess.call([PIN_APP_PATH, "-t", PIN_TOOL_PATH, "--"] + parsec_command.split(" "), env=env_vars)

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
