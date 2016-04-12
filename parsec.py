#!/usr/bin/env python

import datetime
import os
import shlex
import subprocess
import sys
import time
import util

COMMAND_LINE_ARGS = {
  "blackscholes": "1 %(input_file)s %(output_file)s",
  "bodytrack": "%(input_file)s 4 1 5 1 0 1",
  # TODO(saurabh): look into facesim
  # "facesim": ""
  "ferret": "%(input_file)s 5 5 1 %(output_file)s",
  "fluidanimate": "1 1 %(input_file)s %(output_file)s",
  "freqmine": "%(input_file)s 1",
  "raytrace": "%(input_file)s -automove -nthreads 1 -frames 1 -res 1 1",
  # TODO(saurabh): look into swaptions (has no input or output file)
  # "swaptions": ""
  "vips": "im_benchmark %(input_file)s %(output_file)s",
  "x264": "--quiet --qp 20 --partitions b8x8,i4x4 --ref 5 --direct auto --b-pyramid --weightb --mixed-refs --no-fast-pskip --me umh --subme 7 --analyse b8x8,i4x4 --threads 1 -o %(output_file)s %(input_file)s",
}

PARSEC_BASE_DIR = "/afs/ir/users/s/a/saurabh1/research/parsec-3.0"

PIN_OUT_BASE_DIR = "/afs/ir/data/saurabh1/pinatrace_out/"

VALID_INPUT_SIZES = ["test", "simdev", "simsmall", "simmedium", "simlarge", "native"]

def main(app_name, input_size):
  app_base_dir = os.path.join(PARSEC_BASE_DIR, "pkgs/apps", app_name)

  input_tar_path = os.path.join(app_base_dir, "inputs/input_%s.tar" % input_size)

  with util.untar_file(input_tar_path) as input_filename:
    with util.create_tmp_file() as output_filename:
      app_binary_path = os.path.join(app_base_dir, "inst/amd64-linux.gcc/bin", app_name)

      if app_name == "ferret":
        queries_path = os.path.join(os.path.dirname(input_filename.rstrip("/")), "queries")
        input_filename = " ".join([input_filename, "lsh", queries_path])
      elif app_name == "raytrace":
        app_binary_path = os.path.join(app_base_dir, "inst/amd64-linux.gcc/bin", "rtview")
      elif app_name == "vips":
        output_filename = os.path.join(os.path.dirname(output_filename), "parsec.v")

      command_line_args = COMMAND_LINE_ARGS[app_name] % dict(input_file=input_filename, output_file=output_filename)
      parsec_command = "%s %s" % (app_binary_path, command_line_args)

      pin_output_filename = os.path.join(PIN_OUT_BASE_DIR, app_name, "%s_%s.out" % (time.strftime("%Y_%m_%d_%H_%M_%S"), app_name))

      if not os.path.exists(os.path.dirname(pin_output_filename)):
        os.makedirs(os.path.dirname(pin_output_filename))

      pin_process = util.run_under_pin(command_to_run=parsec_command, pin_output_filename=pin_output_filename)

      pin_process_start_time = datetime.datetime.now()

      pin_process.wait()

      pin_process_end_time = datetime.datetime.now()

      symlink_to_latest_output = os.path.join(os.path.dirname(pin_output_filename), "latest")

      if os.path.exists(symlink_to_latest_output):
        os.remove(symlink_to_latest_output)

      os.symlink(pin_output_filename, symlink_to_latest_output)

      header = {
        'app_name': app_name,
        'input_size': input_size,
        'time_ms': ((pin_process_end_time - pin_process_start_time).seconds) * 1000
      }
      util.write_header_to_json_data_file(header, pin_output_filename)

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
