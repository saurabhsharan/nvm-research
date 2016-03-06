#!/usr/bin/env python

import os
import shlex
import subprocess
import sys
import time
import util

MEMCACHED_PORT = "5000"

def main():
  memcached_path = os.path.join(os.getcwd(), "memcached/memcached")
  memcached_command = "%s -p %s" % (memcached_path, MEMCACHED_PORT)

  print "[memcached.py] Starting memcached"
  print "[memcached.py] %s" % memcached_command

  memcached_process = util.run_under_pin(memcached_command, "/dev/null")

  time.sleep(5)

  memcached_process.poll()
  if memcached_process.returncode != None:
    print
    print "[memcached.py] ERROR: memcached failed to start"
    sys.exit(-1)

  mutilate_path = os.path.join(os.getcwd(), "mutilate/mutilate")
  mutilate_command = "%s --server localhost:%s" % (mutilate_path, MEMCACHED_PORT)

  print "[memcached.py] Starting mutilate"
  print "[memcached.py] %s" % mutilate_command
  print
  print

  mutilate_process = subprocess.Popen(shlex.split(mutilate_command))
  mutilate_process.wait()
  time.sleep(1)
  memcached_process.kill()

if __name__ == "__main__":
  main()
