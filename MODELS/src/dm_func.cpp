#include <tadah/models/descriptors/dm/dm_func.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <cmath>

namespace tadah {
namespace models {

F_RLR::F_RLR(const tadah::core::Context &c, size_t cSize, size_t sSize):
  context(&c)
{
  if (!context->get<bool>("INITMB")) return;
  D_Base::get_grid(*context,"SEMBFUNC",sfuncgrid);
  D_Base::get_grid(*context,"CEMBFUNC",cfuncgrid);
  // check if size is correct, throw otherwise
  if (sfuncgrid.size() != sSize) {
    throw std::runtime_error("F_RLR: size of SEMBFUNC does not match declared size.");
  }
  if (cfuncgrid.size() != cSize) {
    throw std::runtime_error("F_RLR: size of CEMBFUNC does not match declared size.");
  }
}
double F_RLR::calc_F(double rho,size_t p) const {
  return sfuncgrid[p]*std::abs(rho)*std::log(cfuncgrid[p]*std::abs(rho) + eps);
}
double F_RLR::calc_dF(double rho,size_t p) const {
  if (rho < 0.0) {
    return -sfuncgrid[p]*(std::log(cfuncgrid[p]*std::abs(rho) + eps) + 1.0);
  } else {
    return sfuncgrid[p]*(std::log(cfuncgrid[p]*std::abs(rho) + eps)+1);
  }
}

//////////////////////////
// F_SQRT
//////////////////////////
F_SQRT::F_SQRT(const tadah::core::Context &c, size_t, size_t sSize):
  context(&c)
{
  if (!context->get<bool>("INITMB")) return;
  if (context->contains("SEMBFUNC") && context->size("SEMBFUNC") > 0) {
    D_Base::get_grid(*context, "SEMBFUNC", sfuncgrid);
    // check if size is correct, throw otherwise
    if (sfuncgrid.size() != sSize) {
      throw std::runtime_error("F_SQRT: size of SEMBFUNC does not match declared size.");
    }
  } else {
    sfuncgrid.resize(sSize);
    for (size_t i=0; i<sSize; ++i) {
      sfuncgrid[i] = 1.0;
    }
  }
}
double F_SQRT::calc_F(double rho,size_t p) const {
    return sfuncgrid[p]*std::sqrt(std::abs(rho) + eps);
}
double F_SQRT::calc_dF(double rho,size_t p) const {
  if (rho < 0.0) {
    return -0.5*sfuncgrid[p]/(std::sqrt(std::abs(rho) + eps));
  } else {
    return 0.5*sfuncgrid[p]/(std::sqrt(rho + eps));
  }
}

//////////////////////////
// F_SQ
//////////////////////////
F_SQ::F_SQ(const tadah::core::Context &c, size_t, size_t sSize):
  context(&c), out_dim(sSize)
{
  if (!context->get<bool>("INITMB")) return;
}
double F_SQ::calc_F(double rho,size_t ) const {
    return rho*rho;
}
double F_SQ::calc_dF(double rho,size_t ) const {
  return 2.0*rho;
}

//////////////////////////
// F_Blip
//////////////////////////
F_Blip::F_Blip(const tadah::core::Context &c, size_t cSize, size_t sSize):
  context(&c)
{
  if (!context->get<bool>("INITMB")) return;
  D_Base::get_grid(*context,"SEMBFUNC",sfuncgrid);
  D_Base::get_grid(*context,"CEMBFUNC",cfuncgrid);
  // F_Blip is a dimension-expanding function: it requires a single
  // radial basis (CGRID/SGRID size 1) but arbitrary CEMBFUNC/SEMBFUNC.
  if (cSize != 1 || sSize != 1) {
    throw std::runtime_error(
        "F_Blip: requires CGRIDMB and SGRIDMB of size 1, got "
        + std::to_string(cSize) + " and " + std::to_string(sSize) + ".");
  }
  if (sfuncgrid.size() != cfuncgrid.size()) {
    throw std::runtime_error(
        "F_Blip: SEMBFUNC and CEMBFUNC must have the same size, got "
        + std::to_string(sfuncgrid.size()) + " and "
        + std::to_string(cfuncgrid.size()) + ".");
  }
}
double F_Blip::calc_F(double rho,size_t p) const {
  double eta = sfuncgrid[p]; 
  double miu = cfuncgrid[p]; 
  return B(rho, eta, miu); 
}
double F_Blip::calc_dF(double rho,size_t p) const {
  return dB(rho, sfuncgrid[p], cfuncgrid[p]);
}

bool F_Blip::calc_dF_dtheta(double rho, size_t p,
                            const std::string& theta_name, int arg_pos,
                            double& out) const
{
  out = 0.0;
  if (theta_name == "CEMBFUNC") {
    if (arg_pos < 1 ||
        static_cast<std::size_t>(arg_pos) > cfuncgrid.size()) return false;
    const std::size_t i = static_cast<std::size_t>(arg_pos - 1);
    if (i != p) return false;
    const double eta = sfuncgrid[p];
    const double mu  = cfuncgrid[p];
    const double x_c = eta * (rho - mu);
    /* dB(eta*(rho-mu))/dmu = dB/dx_c * (-eta) = -dB(x_c, eta). */
    out = -dB(x_c, eta);
    return true;
  }
  if (theta_name == "SEMBFUNC") {
    if (arg_pos < 1 ||
        static_cast<std::size_t>(arg_pos) > sfuncgrid.size()) return false;
    const std::size_t i = static_cast<std::size_t>(arg_pos - 1);
    if (i != p) return false;
    const double eta = sfuncgrid[p];
    const double mu  = cfuncgrid[p];
    if (eta == 0.0) return true;  /* out=0, theta is owned. */
    const double x_c = eta * (rho - mu);
    /* dB/deta = (rho-mu) * dB/dx_c = ((rho-mu)/eta) * dB(x_c, eta). */
    out = (rho - mu) / eta * dB(x_c, eta);
    return true;
  }
  return false;
}

bool F_Blip::calc_d2F(double rho, size_t p, double& out) const
{
  const double eta = sfuncgrid[p];
  const double mu  = cfuncgrid[p];
  const double x_c = eta * (rho - mu);
  /* F'(rho, p) = eta_p * b'(x_c).
   * F''(rho, p) = d/d(rho) [eta * b'(x_c)] = eta^2 * b''(x_c).
   * b''(x_c) is piecewise quadratic; see d_basis_functions.h::d2B. */
  out = eta * eta * d2B(x_c);
  return true;
}

bool F_Blip::calc_dFp_dtheta(double rho, size_t p,
                             const std::string& theta_name, int arg_pos,
                             double& out) const
{
  out = 0.0;
  if (theta_name == "CEMBFUNC") {
    if (arg_pos < 1 ||
        static_cast<std::size_t>(arg_pos) > cfuncgrid.size()) return false;
    const std::size_t i = static_cast<std::size_t>(arg_pos - 1);
    if (i != p) return false;
    const double eta = sfuncgrid[p];
    const double mu  = cfuncgrid[p];
    const double x_c = eta * (rho - mu);
    /* F'(rho, p) = eta * b'(x_c).
     * d/dmu [eta * b'(x_c)] = eta * b''(x_c) * (-eta) = -eta^2 * b''(x_c). */
    out = -eta * eta * d2B(x_c);
    return true;
  }
  if (theta_name == "SEMBFUNC") {
    if (arg_pos < 1 ||
        static_cast<std::size_t>(arg_pos) > sfuncgrid.size()) return false;
    const std::size_t i = static_cast<std::size_t>(arg_pos - 1);
    if (i != p) return false;
    const double eta = sfuncgrid[p];
    const double mu  = cfuncgrid[p];
    const double x_c = eta * (rho - mu);
    /* d/deta [eta * b'(x_c)] = b'(x_c) + eta * b''(x_c) * (rho - mu). */
    out = dB(x_c, 1.0) + eta * (rho - mu) * d2B(x_c);
    return true;
  }
  return false;
}

} // namespace models
} // namespace tadah
