# Ta DFT datasets for the tutorial and examples

DFT data computed by the Tadah! authors.
Licence: the licence of this repository (GPL-3.0).

The Tutorial section [The Ta data][ta-data] accompanies these files.

[ta-data]:
  https://tadah.readthedocs.io/latest/nfp/tutorial/index.html#nfp-ta-data

## Files

| File | Content and use |
|---|---|
| `ta_eos_train.tadahdb` | Equation of state, training |
| `ta_eos_val.tadahdb` | Equation of state, validation |
| `ta_md_train.tadahdb` | Solid molecular dynamics, training |
| `ta_md_val.tadahdb` | Solid molecular dynamics, validation |
| `ta_md_test.tadahdb` | Solid molecular dynamics at unseen pressures, test |
| `ta_vac_train.tadahdb` | Molecular dynamics with a vacancy, training |
| `ta_vac_val.tadahdb` | Molecular dynamics with a vacancy, validation |
| `ta_md_ext_train.tadahdb` | Hot, liquid and high-pressure data, training |
| `ta_md_ext_val.tadahdb` | Hot, liquid and high-pressure data, validation |
| `ta_strain.tadahdb` | Uniaxial compression, test |
| `ta_strain_max20.tadahdb` | Uniaxial compression up to 20 % |
| `ta_elastic.tadahdb` | Small strains and atomic displacements |
| `SHA256SUMS` | SHA-256 checksums of the dataset files |

## Units

Cell vectors and positions are in Å, total energies in eV, forces in eV/Å
and stresses in eV/Å³, positive under tension. These are the stored dataset
units, not the units of every training error scale or validation metric.
