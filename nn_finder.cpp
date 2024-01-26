#include "nn_finder.h"
#include "../LIBS/Eigen/src/Core/Matrix.h"

NNFinder::NNFinder(Config &config):
    cutoff_sq(pow(config.get<double>("RCUTMAX"),2)),
    cutoff(config.get<double>("RCUTMAX"))
{}

void NNFinder::calc(Structure &st) {


    int N[3];
    num_shifts(st, N);

    Eigen::Matrix<double,Eigen::Dynamic,3> shiftedpos(st.natoms(),3);
    // for convenience only:
    std::vector<std::vector<Atom>> &nnatoms = st.near_neigh_atoms;
    std::vector<std::vector<Eigen::Vector3d>> &nnshift = st.near_neigh_shift;
    std::vector<std::vector<size_t>> &nnidx = st.near_neigh_idx;
    nnatoms.resize(st.natoms());
    nnshift.resize(st.natoms());
    nnidx.resize(st.natoms());

    for (int n1=-N[0]; n1<=N[0]; n1++) {
        for (int n2=-N[1]; n2<=N[1]; n2++) {
            for (int n3=-N[2]; n3<=N[2]; n3++) {

                Eigen::Vector3d shift(n1,n2,n3);
                Eigen::Vector3d displacement = st.cell.transpose()*shift;

                for (size_t a=0; a<st.natoms(); ++a) {
                    shiftedpos.row(a) = st(a).position + displacement;
                }

                // calculate all neighbours of a1 for this shift
                for (size_t a1=0; a1<st.natoms(); ++a1) {
                    for (size_t a2=0; a2<st.natoms(); ++a2) {
                        Eigen::Vector3d delij = st(a1).position
                            - shiftedpos.row(a2).transpose();
                        double rij_sq = delij.transpose() * delij;
                        if(rij_sq<cutoff_sq && rij_sq>std::numeric_limits<double>::min()) {
                            Atom atom2 = st(a2);
                            atom2.position = shiftedpos.row(a2);
                            atom2.force.setZero();
                            nnatoms[a1].push_back(atom2);
                            nnidx[a1].push_back(a2);
                            nnshift[a1].push_back(shift);
                        }
                    }
                }
            }
        }
    }
}
void NNFinder::calc(StructureDB &stdb) {
#pragma omp parallel for
    for (size_t s=0; s<stdb.size(); ++s) {
        calc(stdb(s));
    }
}
bool NNFinder::check_box(Structure &st) {
    double f = 1.05;   // extra safety measure
    for (size_t i=0; i<3; ++i)
        if (st.cell.row(i)*st.cell.row(i).transpose() < f*cutoff_sq)
            return false;
    return true;
}
void NNFinder::num_shifts(Structure &st, int N[3]) {
    Eigen::Matrix3d cell_inv = st.cell.completeOrthogonalDecomposition().pseudoInverse();

    double l1 = cell_inv.col(0).norm();
    double l2 = cell_inv.col(1).norm();
    double l3 = cell_inv.col(2).norm();
    
    double f1 = l1 > 0 ? 1.0/l1 : 1.0;
    double f2 = l2 > 0 ? 1.0/l2 : 1.0;
    double f3 = l3 > 0 ? 1.0/l3 : 1.0;

    int b1 = std::max(int(f1/cutoff),1);
    int b2 = std::max(int(f2/cutoff),1);
    int b3 = std::max(int(f3/cutoff),1);

    N[0] = (int)std::round(0.5+cutoff*b1/(f1));
    N[1] = (int)std::round(0.5+cutoff*b2/(f2));
    N[2] = (int)std::round(0.5+cutoff*b3/(f3));

}
