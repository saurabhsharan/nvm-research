#!/usr/bin/env python

import os
import subprocess
import sys
import util

COMMAND_LINE_ARGS = {
  "blackscholes": "1 %s %s"
}

VALID_INPUT_SIZES = ["test", "simdev", "simsmall", "simmedium", "simlarge", "native"]

def main(app_name, input_size):
  app_dir = "parsec-3.0/pkgs/apps/%s/" % app_name

  with util.untar_file(app_dir + "inputs/input_%s.tar" % input_size) as input_filename:
    with util.create_tmp_file() as output_filename:
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
