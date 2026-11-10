#!/usr/bin/env python

"""
Install.py tool to build the Tadah.LAMMPS library.
The library is required by the ML-TADAH package.
To library is built with CMake.

Issue below command from the lammps/src directory:

    make lib-tadah.lammps

"""

from __future__ import print_function
import sys, os, subprocess, shutil
from argparse import ArgumentParser

sys.path.append('..')
from install_helpers import get_cpus, fullpath

parser = ArgumentParser(prog='Install.py',
                        description="LAMMPS Tadah.MD library build wrapper script")

args = parser.parse_args()

build_dir='build'
cwd = fullpath('.')
lib = os.path.basename(cwd)

# make the library with parallel make
n_cpus = get_cpus()

# Build with CMake
cmd = "mkdir -p build && cd build && cmake .."
cmd += " -DBUILD_SHARED_LIBS=OFF" 
cmd += " && make -j %d" % n_cpus

try:
  txt = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT)
  #print(txt.decode('UTF-8'))
except subprocess.CalledProcessError as e:
  print("Build failed with:\n %s" % e.output.decode('UTF-8'))
  sys.exit(1)

# Install with make does not work because
# compilation of cmrc resource library is not implemented
# The possible workaround is to generate intermediate files
# with cmake and then compile and link resource lib within the Makefile
#makefile=os.path.join(cwd,'Makefile')
#
#if not os.path.exists(makefile):
#  sys.exit("lib/%s/Makefile.%s does not exist" % (lib, machine))
#
#print("Configuring lib%s..." % lib)
#cmd = "./configure"
#try:
#  txt = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT)
#  print(txt.decode('UTF-8'))
#except subprocess.CalledProcessError as e:
#  print("Configure failed with:\n %s" % e.output.decode('UTF-8'))
#  sys.exit(1)
#
#print("Building lib%s..." % lib)
#cmd = "make -f %s clean; make -f %s -j%d" % (makefile, makefile, n_cpus)
#try:
#  txt = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT)
#  print(txt.decode('UTF-8'))
#except subprocess.CalledProcessError as e:
#  print("Make failed with:\n %s" % e.output.decode('UTF-8'))
#  sys.exit(1)


# Verify compilation
if os.path.exists("%s/lib%s.a" % (os.path.join(cwd,build_dir),lib)):
  print("Build was successful.")
else:
  sys.exit("Build of lib/%s was NOT successful" % (lib, lib))

# Verifying that ML-TADAH is not already in the src directory
if "ML-TADAH" in os.listdir("../../src/"):
    print("Error. ML-TADAH already exists as a directory within the /src/ folder.")
    print("Please remove ML-TADAH from ../../src using rm -r ../../src/ML-TADAH and try again.")
    sys.exit(1)
# Copy ML-TADAH to lammps src
try:
    print("Copying ML-TADAH directory to LAMMPS src directory...")
    shutil.copytree("ML-TADAH", "../../src/ML-TADAH")
    print("Done")
except:
    print("Could not copy ML-TADAH, please:")
    print("1) Verify the python module 'shutil' is available")
    print("2) Verify that tadah.lammps is in lammps/lib")
    sys.exit(1)

