#!/bin/bash

# This script works with orthogonal boxes only.
# Shifts box origin to (0,0,0) and move all atoms
# to the new box.
# Constant volume is assumed.
#
# Usage:
# this_script_name lammps_dump_file lammps_log_file > outfile.tadah
#
# The first line of the log file corresponds to the first
# structure in a dump file. The second line to the second structure, etc...
# enecol variable must point to the potential energy column in a log file.
# same for stress tensor.
# 
# logfile thermo_style:
# compute S all stress/atom NULL virial
# compute 1 all reduce sum c_S[1] c_S[2] c_S[3] c_S[4] c_S[5] c_S[6]
# thermo_style    custom step temp vol pe ke etotal press c_1[1] c_1[2] c_1[3] c_1[4] c_1[5] c_1[6]
#
# Note that stress/atom outputs stress*volume term so we have to divide by cell volume
#
# dump file must be generated with this setting:
#    dump            4 all custom ${THERMO} db.dat element x y z fx fy fz
#    dump_modify     4 element Ti sort id

dumpfile=$1
logfile=$2
skip=$(grep -n -m 1 "Step" $logfile |sed  's/\([0-9]*\).*/\1/') # logfile num of header lines
natoms=$(grep -m 1 -A 1 "ITEM: NUMBER OF ATOMS" $dumpfile | tail -1)   # number of atoms in a LAMMPS dump file
header=$(grep -n -m 1 "ITEM: ATOMS" $dumpfile |sed  's/\([0-9]*\).*/\1/')    # number of lines in a lammps dump before atom positions are listed
enecol=4    # energy column in a lammps log file
sxxcol=8    # column for s_xx component of a virial stress tensor in press*volume units in a logfile
syycol=9    # column for s_yy component of a virial stress tensor in press*volume units in a logfile
szzcol=10   # column for s_zz component of a virial stress tensor in press*volume units in a logfile
sxycol=11   # column for s_xy component of a virial stress tensor in press*volume units in a logfile
sxzcol=12   # column for s_xz component of a virial stress tensor in press*volume units in a logfile
syzcol=13   # column for s_yz component of a virial stress tensor in press*volume units in a logfile
cellx=$(grep -n -m 1 -A 1 "ITEM: BOX BO" $dumpfile | sed  's/\([0-9]*\).*/\1/' | tail -1) # line number for xlow xhigh in a dump file
celly=$(($cellx+1))     # line number for ylow yhigh in a dump file
cellz=$(($cellx+2))     # line number for zlow zhigh in a dump file


s=1 # structure counter
i=1 # line counter
while IFS= read -r line
do

  if [[ "$i" -eq $cellx ]]; then
    xlow=$(echo $line | cut -d " " -f 1)
    xhigh=$(echo $line | cut -d " " -f 2)
  fi
  if [[ "$i" -eq $celly ]]; then
    ylow=$(echo $line | cut -d " " -f 1)
    yhigh=$(echo $line | cut -d " " -f 2)
  fi
  if [[ "$i" -eq $cellz ]]; then
    zlow=$(echo $line | cut -d " " -f 1)
    zhigh=$(echo $line | cut -d " " -f 2)
  fi
  if [[ "$i" -eq "$header" ]]; then
    # Calculate lengths
    Lx=$(echo "$xhi - $xlo" | bc -l)
    Ly=$(echo "$yhi - $ylo" | bc -l)
    Lz=$(echo "$zhi - $zlo" | bc -l)

    # Calculate volume
    volume=$(echo "$Lx * $Ly * $Lz" | bc -l)

    echo "Structure " $s
    thermoline=$(sed "$(($s+$skip))q;d" $logfile)
    echo $thermoline | cut -d " " -f $enecol
    echo "$(awk -v xlow="$xlow" -v xhigh="$xhigh" 'BEGIN{printf "%.13f", xhigh-xlow}')  0.0  0.0"
    echo "0.0  $(awk -v ylow="$ylow" -v yhigh="$yhigh" 'BEGIN{printf "%.13f", yhigh-ylow}')  0.0"
    echo "0.0  0.0  $(awk -v zlow="$zlow" -v zhigh="$zhigh" 'BEGIN{printf "%.13f", zhigh-zlow}')"

    sxx=$(echo $thermoline | cut -d " " -f $sxxcol)
    syy=$(echo $thermoline | cut -d " " -f $syycol)
    szz=$(echo $thermoline | cut -d " " -f $szzcol)
    sxy=$(echo $thermoline | cut -d " " -f $sxycol)
    sxz=$(echo $thermoline | cut -d " " -f $sxzcol)
    syz=$(echo $thermoline | cut -d " " -f $syzcol)

    # convert from bars*angstrom^3 to eV
    # conversion factor 6.241509744511500288*10^-7
    sxx=$(echo "$sxx $volume" | awk '{printf "%f", 6.241509744511500288*10^-7*$1/$2}')
    syy=$(echo "$syy $volume" | awk '{printf "%f", 6.241509744511500288*10^-7*$1/$2}')
    szz=$(echo "$szz $volume" | awk '{printf "%f", 6.241509744511500288*10^-7*$1/$2}')
    sxy=$(echo "$sxy $volume" | awk '{printf "%f", 6.241509744511500288*10^-7*$1/$2}')
    sxz=$(echo "$sxz $volume" | awk '{printf "%f", 6.241509744511500288*10^-7*$1/$2}')
    syz=$(echo "$syz $volume" | awk '{printf "%f", 6.241509744511500288*10^-7*$1/$2}')

    echo $sxx $sxy $sxz
    echo $sxy $syy $syz
    echo $sxz $syz $szz
    s=$(($s+1))
  fi
  if [[ "$i" -gt "$header" ]]; then
    echo "$line" | awk -v xlow="$xlow" -v ylow="$ylow" -v zlow="$zlow" -v xhigh="$xhigh" -v yhigh="$yhigh" -v zhigh="$zhigh" '{printf "%s %.13f %.13f %.13f %.13f %.13f %.13f\n", $1,$2-xlow,$3-ylow,$4-zlow,$5,$6,$7}'
  fi
  if [[ "$i" -eq $(($natoms + $header)) ]]; then
    i=0
    echo ""
  fi

  i=$(($i+1))
done < "$dumpfile"
