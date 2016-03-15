#!/usr/bin/env python

import os
import shlex
import subprocess
import sys
import time
import util

MEMCACHED_PORT = "5000"

PIN_OUT_BASE_DIR = "/afs/ir/data/saurabh1/pinatrace_out/"

def main():
  memcached_path = os.path.join(os.getcwd(), "memcached/memcached")
  memcached_command = "%s -p %s" % (memcached_path, MEMCACHED_PORT)

  print "[memcached.py] Starting memcached"
  print "[memcached.py] %s" % memcached_command

  pin_output_filename = os.path.join(PIN_OUT_BASE_DIR, "memcached", "%s_memcached.out" % time.strftime("%Y_%m_%d_%H_%M_%S"))

  if not os.path.exists(os.path.dirname(pin_output_filename)):
    os.makedirs(os.path.dirname(pin_output_filename))

  memcached_process = util.run_under_pin(memcached_command, pin_output_filename)

  time.sleep(5)

  memcached_process.poll()
  if memcached_process.returncode != None:
    print
    print "[memcached.py] ERROR: memcached failed to start"
    sys.exit(-1)

  mutilate_path = os.path.join(os.getcwd(), "mutilate/mutilate")
  mutilate_command = "%s --server localhost:%s --valuesize=10000 --records=1000000 --time=20" % (mutilate_path, MEMCACHED_PORT)

  print "[memcached.py] Starting mutilate"
  print "[memcached.py] %s" % mutilate_command
  print
  print

  mutilate_process = subprocess.Popen(shlex.split(mutilate_command))
  mutilate_process.wait()
  time.sleep(1)
  memcached_process.terminate()

  symlink_to_latest_output = os.path.join(os.path.dirname(pin_output_filename), "latest")

  if os.path.exists(symlink_to_latest_output):
    os.remove(symlink_to_latest_output)

  os.symlink(pin_output_filename, symlink_to_latest_output)

  print "[memcached.py] added symlink"

  with open("memcached_log.txt") as f:
    f.write("%s => %s" % (mutilate_command, pin_output_filename))

if __name__ == "__main__":
  main()
  print "[memcached.py] finished main()"
