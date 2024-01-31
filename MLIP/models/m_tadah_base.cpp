#include "m_tadah_base.h"
#include "../../CORE/registry.h"
#include "../design_matrix/functions/dm_function_base.h"


void M_Tadah_Base::fpredict(const size_t a, force_type &v, const StDescriptors &std, const Structure &st)
{
    for (size_t jj=0; jj<std.fd[a].size(); ++jj) {
        size_t j=st.near_neigh_idx[a][jj];
        const size_t aa = st.get_nn_iindex(a,j,jj);
        const fd_type &fdji = std.fd[j][aa];
        const fd_type &fdij = std.fd[a][jj];
        const aed_rctype aedi = std.get_aed(a);
        const aed_rctype aedj = std.get_aed(j);
        v += fpredict(fdij,aedi);
        v -= fpredict(fdji,aedj);
    }
}
force_type M_Tadah_Base::fpredict(const size_t a, const StDescriptors &std, const Structure &st)
{
    force_type v(0,0,0);
    fpredict(a,v,std,st);
    return v;
}
double M_Tadah_Base::epredict(const StDescriptors &std)
{
    double energy = 0;
    for (size_t i=0;i<std.naed();++i) {
        energy+=epredict(std.get_aed(i));
    }
    return energy;
}

Structure M_Tadah_Base::predict(const Config &c, StDescriptors &std, const Structure &st)
{
    if(c.get<bool>("NORM") && !std.normalised
            && c("MODEL")[1].find("Linear") == std::string::npos)
        norm.normalise(std);
    Structure st_(st);
    st_.energy = epredict(std);
    if (c.get<bool>("FORCE")) {
        for (size_t a=0; a<st.natoms(); ++a) {
            st_.atoms[a].force = fpredict(a,std,st);
        }
    }
    if (c.get<bool>("STRESS")) {
        st_.stress = spredict(std,st);
    }
    return st_;
}

StructureDB M_Tadah_Base::predict(const Config &c, StDescriptorsDB &st_desc_db, const StructureDB &stdb)
{
    StructureDB stdb_;
    stdb_.structures.resize(stdb.size());
#pragma omp parallel for
    for (size_t i=0; i<stdb.size(); ++i) {
        stdb_(i) = predict(c,st_desc_db(i), stdb(i));
    }
    return stdb_;
}

StructureDB M_Tadah_Base::predict(Config &c, const StructureDB &stdb, DC_Base &dc)
{
    StructureDB stdb_;
    stdb_.structures.resize(stdb.size());
#pragma omp parallel for
    for (size_t i=0; i<stdb.size(); ++i) {
        StDescriptors st_d = dc.calc(stdb(i));
        stdb_(i) = predict(c,st_d, stdb(i));
    }
    return stdb_;
}

stress_type M_Tadah_Base::spredict(const StDescriptors &std, const Structure &st)
{
    stress_type s;
    s.setZero();
    for (size_t a=0; a<st.natoms(); ++a) {
        s += spredict(a,std,st);
    }
    return s;
}
stress_type M_Tadah_Base::spredict(const size_t a, const StDescriptors &std, const Structure &st)
{
    stress_type s;
    s.setZero();
    spredict(a,s,std,st);
    return s;
}
void M_Tadah_Base::spredict(const size_t a, stress_type &s, const StDescriptors &std, const Structure &st)
{
    const Vec3d &ri = st.atoms[a].position;
    const aed_rctype aedi = std.get_aed(a);
    for (size_t jj=0; jj<std.fd[a].size(); ++jj) {
        size_t j=st.near_neigh_idx[a][jj];
        const size_t aa = st.get_nn_iindex(a,j,jj);
        const Vec3d &rj = st.nn_pos(a,jj);
        const fd_type &fdij = std.fd[a][jj];
        const fd_type &fdji = std.fd[j][aa];
        const aed_rctype aedj = std.get_aed(j);
        const force_type fij = fpredict(fdij,aedi);
        const force_type fji = fpredict(fdji,aedj);
        const force_type fIJ = fij - fji;
        // fJI = -fIJ

        s -= 0.5*(Eigen::Map<const Eigen::Vector3d>(ri.v)
                -Eigen::Map<const Eigen::Vector3d>(rj.v))
            *(Eigen::Map<const Eigen::Vector3d>(fIJ.v)).transpose();
    }
    //for(size_t i=0;i<6;++i) s(i) *= s_std_dev[i];

}
