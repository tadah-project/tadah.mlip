# The build file of the package, which the LAMMPS configure step reads
# through integration/TadahLAMMPSProjectInclude.cmake. It adds the library
# of the pair style (lammps/), which adds md, models and core in turn, each
# from its directory here.
add_subdirectory(lammps)
