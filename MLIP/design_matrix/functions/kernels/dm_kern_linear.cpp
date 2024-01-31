#include "dm_kern_linear.h"

Registry<DM_Function_Base>::Register<DM_Kern_Linear> DM_Kern_Linear_1( "Kern_Linear" );
Registry<DM_Function_Base,Config&>::Register<DM_Kern_Linear> DM_Kern_Linear_2( "Kern_Linear" );

DM_Kern_Linear::DM_Kern_Linear() {}
DM_Kern_Linear::DM_Kern_Linear (const Config &c): Kern_Linear(c)
{}
size_t DM_Kern_Linear::get_phi_cols(const Config &config)
{
    size_t cols = config.get<size_t>("DSIZE");
    return cols;
}
void DM_Kern_Linear::calc_phi_energy_row(phi_type &Phi, size_t &row,
        const double fac, const Structure &, const StDescriptors &st_d)
{
    for (size_t a=0; a<st_d.naed();++a) {
        const aed_type2 &aed = st_d.get_aed(a);  // TODO
        //Eigen::VectorXd t(aed.data.data());
        for (size_t j=0; j<aed.size(); ++j) {
            Phi.row(row)[j]+=aed[j];
        }
        //Phi.row(row) += Eigen::Map<const Eigen::VectorXd>(aed.data.data());
        //Phi.row(row) += Eigen::Map<const Eigen::Matrix<double,aed.size(),1>>(aed.data.data());
    }
    Phi.row(row++) *= fac;
}
void DM_Kern_Linear::calc_phi_force_rows(phi_type &Phi, size_t &row,
        const double fac, const Structure &st, const StDescriptors &st_d)
{
    for (size_t a=0; a<st.natoms(); ++a) {
        for (size_t jj=0; jj<st_d.fd[a].size(); ++jj) {
            const size_t j=st.near_neigh_idx[a][jj];
            const size_t aa = st.get_nn_iindex(a,j,jj);
            for (size_t k=0; k<3; ++k) {
                //Phi.row(row+k) -= fac*(st_d.fd[a][jj].col(k)-
                //        st_d.fd[j][aa].col(k));
                aed_type2 temp = fac*(st_d.fd[a][jj].col(k)-
                        st_d.fd[j][aa].col(k));
                for (size_t d=0; d<temp.size(); ++d) {
                    Phi.row(row+k)[d] -= temp[d];
                }
            }
        }
        row+=3;
    }

}
void DM_Kern_Linear::calc_phi_stress_rows(phi_type &Phi, size_t &row,
        const double fac[6], const Structure &st, const StDescriptors &st_d)
{
    for (size_t i=0; i<st.natoms(); ++i) {
        const Vec3d &ri = st(i).position;
        for (size_t jj=0; jj<st_d.fd[i].size(); ++jj) {
            const size_t j=st.near_neigh_idx[i][jj];
            const size_t ii = st.get_nn_iindex(i,j,jj);
            const fd_type &fdij = st_d.fd[i][jj];
            const fd_type &fdji = st_d.fd[j][ii];
            const Vec3d &rj = st.nn_pos(i,jj);
            size_t mn=0;
            for (size_t x=0; x<3; ++x) {
                for (size_t y=x; y<3; ++y) {
                    //Phi.row(row+mn) += 0.5*fac[mn]*(ri(x)-rj(x))*(fdij.col(y)-fdji.col(y));
                    aed_type2 temp = 0.5*fac[mn]*(ri(x)-rj(x))*(fdij.col(y)-fdji.col(y));
                    for (size_t d=0; d<temp.size(); ++d) {
                        Phi.row(row+mn)[d] += temp[d];
                    }
                    mn++;
                }
            }
        }
    }
    row += 6;
}
