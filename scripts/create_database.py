import sys
from tqdm import tqdm
import numpy as np
from pathlib import Path
from pymatgen.io.vasp import Vasprun

if (len(sys.argv) != 3):
    sys.exit("Usage: %s DATA_DIR OUTFILE\n" % sys.argv[0])

# get all absolute paths to vasprun.xml in DATA_DIR
paths = [path.absolute() for path in Path(sys.argv[1]).rglob('vasprun.xml')]

outfile = open(sys.argv[2],"w")

for i,p in enumerate(tqdm(paths)):
    outfile.write("#%d %s\n" % (i+1,p))
    v = Vasprun(p, parse_eigen=False, parse_dos=False, parse_potcar_file=False)
    # write energy - free energy is consistent with Hellmann-Feynman forces
    #outfile.write("%.10f\n" % v.final_energy)
    outfile.write("%.10f\n" % v.ionic_steps[0]["e_fr_energy"])
    # write cell
    np.savetxt(outfile,v.structures[0].lattice.matrix,fmt='%.10f')
    # write stress in Ev units (pressure*volume)
    # vasp stress in Kb, volume in Angstrom
    # convert from kbars*angstrom^3 to eV
    # conversion factor 6.241509744511500288*10^-4
    volume = (6.241509744511500288*10.0**-4)*v.structures[0].lattice.volume
    # stress convenction:
    # VASP  negative sign below
    # LAMMPS positive sign below
    stress = -np.asarray(v.ionic_steps[0]["stress"])*volume
    np.savetxt(outfile,stress,fmt='%.10f')
    # write Element px py px fx fy fz
    for sym,site,f in zip(v.structures[0].species, v.structures[0].sites, v.ionic_steps[0]['forces']):
        outfile.write("%s    %11.10f  %11.10f  %11.10f    %11.10f  %11.10f  %11.10f\n" %
        (sym.symbol,site.coords[0],site.coords[1],site.coords[2],f[0],f[1],f[2]))
    # Finished printing structure - add new line
    outfile.write("\n")
outfile.close()
