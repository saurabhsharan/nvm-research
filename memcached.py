#!/usr/bin/env python

import os
import shlex
import signal
import subprocess
import sys
import time
import util
import datetime

MEMCACHED_PORT = "6977"

RESEARCH_DIR = "/afs/ir/users/s/a/saurabh1/research"

PIN_OUT_BASE_DIR = "/afs/ir/data/saurabh1/pinatrace_out/"

# TODO(saurabh): use helper functions w/ exceptions to clean up control flow?
def main():
  memcached_path = os.path.join(RESEARCH_DIR, "memcached/memcached")
  # TODO(saurabh): make -m value configurable?
  memcached_command = "%s -p %s -m 10000" % (memcached_path, MEMCACHED_PORT)

  print "[memcached.py] Starting memcached"
  print "[memcached.py] %s" % memcached_command

  pin_output_filename = os.path.join(PIN_OUT_BASE_DIR, "memcached", "%s_memcached.out" % time.strftime("%Y_%m_%d_%H_%M_%S"))

  if not os.path.exists(os.path.dirname(pin_output_filename)):
    os.makedirs(os.path.dirname(pin_output_filename))

  pin_process = util.run_under_pin(memcached_command, pin_output_filename, child_injection=True)

  print "[memcached.py] pin process pid = %s" % pin_process.pid

  # HACK(saurabh): wait for memcached to possibly fail (usually because TCP port is already in use)
  time.sleep(3)
  pin_process.poll()
  if pin_process.returncode != None:
    print
    print "[memcached.py] ERROR: memcached failed to start"
    sys.exit(-1)

  valuesize, records, time_sec = sys.argv[1], sys.argv[2], sys.argv[3]

  mutilate_path = os.path.join(RESEARCH_DIR, "mutilate/mutilate")
  mutilate_command = "%s --server localhost:%s --verbose --valuesize=%s --records=%s --time=%s" % (mutilate_path, MEMCACHED_PORT, valuesize, records, time_sec)

  print "[memcached.py] Starting mutilate"
  print "[memcached.py] %s" % mutilate_command
  print
  print

  mutilate_start_time = datetime.datetime.now()

  mutilate_process = subprocess.Popen(shlex.split(mutilate_command))
  mutilate_process.wait()

  if mutilate_process.returncode != 0:
    print "[memcached.py] ERROR: mutilate process failed"

    # TODO(saurabh): this clean-up really belongs as part of pin module
    if os.path.exists(pin_output_filename):
      print "[memcached.py] Deleting %s" % pin_output_filename
      os.remove(pin_output_filename)

    pin_process.terminate()

    time.sleep(2)

    sys.exit(-1)

  print "[memcached.py] mutilate process finished"

  mutilate_end_time = datetime.datetime.now()

  time.sleep(1)

  pin_children_pids = util.get_children_pids(pin_process.pid)
  assert len(pin_children_pids) == 1

  memcached_process_pid = int(pin_children_pids[0])

  os.kill(memcached_process_pid, signal.SIGTERM)

  time.sleep(3)

  pin_process.wait()

  print "[memcached.py] terminated memcached process"

  symlink_to_latest_output = os.path.join(os.path.dirname(pin_output_filename), "latest")

  if os.path.lexists(symlink_to_latest_output):
    if not os.path.exists(symlink_to_latest_output):
      print "[memcached.py] WARNING: Broken symlink points to bad file %s" % os.path.realpath(symlink_to_latest_output)
    os.remove(symlink_to_latest_output)

  os.symlink(pin_output_filename, symlink_to_latest_output)

  print "[memcached.py] added symlink"

  with open("memcached_log5.txt", 'a') as f:
    f.write("%s\n%s\n%r sec\n\n" % (mutilate_command[len(os.path.dirname(mutilate_path)):], pin_output_filename[len(os.path.dirname(pin_output_filename)):], (mutilate_end_time - mutilate_start_time).seconds))

  print "[memcached.py] wrote command to log file: %s" % pin_output_filename

if __name__ == "__main__":
  main()
  print "[memcached.py] finished main()"
