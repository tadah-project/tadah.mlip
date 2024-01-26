# require pot.tadah file to be present in a script working directory
units           metal
atom_style      atomic
newton          on
boundary        p p p

lattice         bcc 3.5
region          box block 0 5 0 5 0 5
create_box      1 box
create_atoms    1 box

mass            1 180.95 # Ta

timestep        0.003
pair_style      tadah/tadah
pair_coeff      * * pot.tadah Ta

neighbor        1.0 bin
neigh_modify    every 3 delay 0 check yes

velocity        all  create 1000 4981299 dist gaussian

thermo          100
thermo_style    custom step temp vol press pe etotal

fix             fixNVE all nve
run             1000
