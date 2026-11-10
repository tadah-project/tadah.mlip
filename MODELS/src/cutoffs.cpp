#include <tadah/models/cutoffs.h>
#include <cmath>
#include <stdexcept>
namespace tadah {
namespace models {

template<>
tadah::core::Registry<Cut_Base,double&>::Map& tadah::core::Registry<Cut_Base,double&>::registry =
  tadah::core::Registry<tadah::models::Cut_Base,double&>::instance();
tadah::core::Registry<Cut_Base,double&>::Register<Cut_Cos> Cut_Cos_1( "Cut_Cos" );
tadah::core::Registry<Cut_Base,double&>::Register<Cut_CosS> Cut_Cos_2( "Cut_CosS" );
tadah::core::Registry<Cut_Base,double&>::Register<Cut_Dummy> Cut_Dummy_1( "Cut_Dummy" );
tadah::core::Registry<Cut_Base,double&>::Register<Cut_Poly> Cut_Poly_1( "Cut_Poly" );
tadah::core::Registry<Cut_Base,double&>::Register<Cut_PolyS> Cut_Poly_2( "Cut_PolyS" );
tadah::core::Registry<Cut_Base,double&>::Register<Cut_PT> Cut_PT_1( "Cut_PT" );
tadah::core::Registry<Cut_Base,double&>::Register<Cut_SinInv> Cut_SinInv_1( "Cut_SinInv" );
tadah::core::Registry<Cut_Base,double&>::Register<Cut_Tanh> Cut_Tanh_1( "Cut_Tanh" );


Cut_Base::~Cut_Base() {}
void Cut_Base::test_rcut(const double r)
{
    if (r<0.0)
        throw std::runtime_error("Cutoff distance cannot be negative.");
}

Cut_Cos::Cut_Cos() {}
Cut_Cos::Cut_Cos(double rcut)
{
    set_rcut(rcut);
    test_rcut(rcut);
}

std::string Cut_Cos::label() {
    return lab;
}

void Cut_Cos::set_rcut(const double r) {
    test_rcut(r);
    rcut=r;
    rcut_sq=r*r;
    rcut_inv= r<=0 ? 0.0 : 1.0/r;
}
double Cut_Cos::get_rcut() {
    return rcut;
}

double Cut_Cos::get_rcut_sq() {
    return rcut_sq;
}

double Cut_Cos::calc(double r) {
    if (r>=rcut) return 0.0;
    return 0.5 * (std::cos(M_PI*r*rcut_inv) + 1.0);
}
double Cut_Cos::calc_prime(double r) {
    if (r>=rcut) return 0.0;
    double t = M_PI*rcut_inv;
    return -0.5 * t*sin(t*r);

}
double Cut_Cos::calc_drcut(double r) {
    // f_c(r) = (1/2)[cos(pi r / rcut) + 1]
    // df_c/drcut = (1/2) * (-sin(pi r / rcut)) * (-pi r / rcut^2)
    //            = (pi r / (2 rcut^2)) * sin(pi r / rcut)
    if (r>=rcut) return 0.0;
    const double t = M_PI * rcut_inv;          // pi / rcut
    return 0.5 * t * rcut_inv * r * std::sin(t*r);
}
double Cut_Cos::calc_prime_drcut(double r) {
    // calc_prime(r) = -0.5 * (pi/rcut) * sin(pi r / rcut)
    //              = -0.5 * t * sin(t*r),   t = pi/rcut
    // d(calc_prime)/drcut
    //   = -0.5 * [dt/drcut * sin(t r) + t * cos(t r) * d(t r)/drcut]
    //   = -0.5 * [(-pi/rcut^2) sin(t r) + t cos(t r) * (-pi r / rcut^2)]
    //   = 0.5 * (pi/rcut^2) * [sin(t r) + (t r) cos(t r)]
    if (r>=rcut) return 0.0;
    const double t = M_PI * rcut_inv;          // pi / rcut
    const double tr = t * r;
    return 0.5 * t * rcut_inv * (std::sin(tr) + tr * std::cos(tr));
}
double Cut_Cos::calc_double_prime(double r) {
    // f(r)   = (1/2)[cos(t r) + 1],   t = pi/rcut
    // f'(r)  = -(t/2) sin(t r)
    // f''(r) = -(t^2 / 2) cos(t r)
    if (r>=rcut) return 0.0;
    const double t = M_PI * rcut_inv;
    return -0.5 * t * t * std::cos(t * r);
}

Cut_Dummy::Cut_Dummy() {}
Cut_Dummy::Cut_Dummy(double rcut)
{
    set_rcut(rcut),
    test_rcut(rcut);
}

std::string Cut_Dummy::label() {
    return lab;
}

void Cut_Dummy::set_rcut(const double r) {
    test_rcut(r);
    rcut=r;
    rcut_sq=r*r;
    rcut_inv= r<=0 ? 0.0 : 1.0/r;
}
double Cut_Dummy::get_rcut() {
    return rcut;
}

double Cut_Dummy::get_rcut_sq() {
    return rcut_sq;
}

double Cut_Dummy::calc(double r) {
    if (r>=rcut) return 0.0;
    return 1.0;
}
double Cut_Dummy::calc_prime(double) {
    return 0.0;
}

Cut_Tanh::Cut_Tanh() {}
Cut_Tanh::Cut_Tanh(double rcut)
{
    set_rcut(rcut);
}

std::string Cut_Tanh::label() {
    return lab;
}

void Cut_Tanh::set_rcut(const double r) {
    test_rcut(r);
    rcut=r;
    rcut_sq=r*r;
    rcut_inv= r<=0 ? 0.0 : 1.0/r;
}
double Cut_Tanh::get_rcut() {
    return rcut;
}

double Cut_Tanh::get_rcut_sq() {
    return rcut_sq;
}

double Cut_Tanh::calc(double r) {
    if (r>=rcut) return 0.0;
    double t = std::tanh(1.0 - r*rcut_inv);
    return t*t*t;
}
double Cut_Tanh::calc_prime(double r) {
    if (r>=rcut) return 0.0;
    double a = (1.0-r*rcut_inv);
    double t = std::tanh(a);
    double s = 1.0/(std::cosh(a));
    return -3.0*t*t*rcut_inv*s*s;
}
double Cut_Tanh::calc_drcut(double r) {
    // f_c(r) = tanh^3(u),  u = 1 - r/rcut
    // df/drcut = 3 tanh^2(u) sech^2(u) * du/drcut,  du/drcut = r/rcut^2
    if (r>=rcut) return 0.0;
    double a = 1.0 - r*rcut_inv;
    double t = std::tanh(a);
    double s = 1.0/std::cosh(a);
    return 3.0*t*t*s*s*r*rcut_inv*rcut_inv;
}
double Cut_Tanh::calc_prime_drcut(double r) {
    // calc_prime(r) = -3 tanh^2(u) sech^2(u) / rcut,  u = 1 - r/rcut
    // Let t = tanh(u), s = sech(u). du/drcut = r/rcut^2.
    // dt/du = s^2;  ds/du = -t s.
    // d(t^2 s^2)/du = 2 t s^2 dt/du + t^2 * 2 s ds/du
    //              = 2 t s^4 - 2 t^3 s^2 = 2 t s^2 (s^2 - t^2)
    // d(-3 t^2 s^2 / rcut)/drcut
    //   = (3 t^2 s^2 / rcut^2)
    //     + (-3/rcut) * 2 t s^2 (s^2 - t^2) * (r/rcut^2)
    //   = (3 t s^2 / rcut^2) * [t - 2 r (s^2 - t^2) / rcut]
    if (r>=rcut) return 0.0;
    double a = 1.0 - r*rcut_inv;
    double t = std::tanh(a);
    double s = 1.0/std::cosh(a);
    double t2 = t*t;
    double s2 = s*s;
    return 3.0 * t * s2 * rcut_inv * rcut_inv
           * (t - 2.0 * r * (s2 - t2) * rcut_inv);
}
double Cut_Tanh::calc_double_prime(double r) {
    // f(r)  = tanh^3(u),  u = 1 - r/rcut,  du/dr = -1/rcut
    // f'(r) = -3 t^2 s^2 / rcut,  t = tanh(u), s = sech(u)
    // Let g(u) = t^2 s^2.
    //   dg/du = 2 t s^2 dt/du + t^2 * 2 s ds/du
    //         = 2 t s^4 + 2 t^2 s * (-t s) = 2 t s^2 (s^2 - t^2)
    // f''(r) = (-3/rcut) * dg/du * du/dr
    //        = (-3/rcut) * 2 t s^2 (s^2 - t^2) * (-1/rcut)
    //        = (6/rcut^2) t s^2 (s^2 - t^2)
    if (r>=rcut) return 0.0;
    double a = 1.0 - r*rcut_inv;
    double t = std::tanh(a);
    double s = 1.0/std::cosh(a);
    double t2 = t*t;
    double s2 = s*s;
    return 6.0 * rcut_inv * rcut_inv * t * s2 * (s2 - t2);
}

Cut_PolyS::Cut_PolyS() {}
Cut_PolyS::Cut_PolyS(double rcut)
{
    if (rcut<=1.0)
        throw std::runtime_error(
                "Cut_PolyS cutoff distance cannot be smaller than 1.0.");
    set_rcut(rcut);
    test_rcut(rcut);
}

std::string Cut_PolyS::label() {
    return lab;
}

void Cut_PolyS::set_rcut(const double r) {
    test_rcut(r);
    rcut=r;
    rcut_sq=r*r;
    rcut_inv= r<=0 ? 0.0 : 1.0/r;
    rcut_inner=rcut-1.0;
}
double Cut_PolyS::get_rcut() {
    return rcut;
}

double Cut_PolyS::get_rcut_sq() {
    return rcut_sq;
}

double Cut_PolyS::calc(double r) {
    if (r>=rcut) return 0.0;
    else if (r<= rcut_inner) return 1.0;
    double rs=r-rcut_inner;
    return rs*rs*rs*(rs*(15.0-6.0*rs)-10.0)+1;
}
double Cut_PolyS::calc_prime(double r) {
    if (r>=rcut) return 0.0;
    else if (r<= rcut_inner) return 0.0;
    double rs=r-rcut_inner;
    return -30.0*(rs-1.0)*(rs-1.0)*rs*rs;
}
double Cut_PolyS::calc_drcut(double r) {
    // rs = r - rcut_inner = r - (rcut - 1) = r - rcut + 1
    // f depends on rcut only through rs; drs/drcut = -1
    // df/drcut = dg/drs * (-1) = -calc_prime(r)
    if (r>=rcut) return 0.0;
    else if (r<= rcut_inner) return 0.0;
    double rs = r - rcut_inner;
    return 30.0*(rs-1.0)*(rs-1.0)*rs*rs;
}
double Cut_PolyS::calc_prime_drcut(double r) {
    // calc_prime(r) = -30 (rs-1)^2 rs^2 = h(rs),  rs = r - (rcut-1)
    // drs/drcut = -1
    // h(rs) = -30 (rs^4 - 2 rs^3 + rs^2)
    // h'(rs) = -120 rs^3 + 180 rs^2 - 60 rs
    // d(calc_prime)/drcut = h'(rs) * drs/drcut = -h'(rs)
    //                     = 120 rs^3 - 180 rs^2 + 60 rs
    //                     = 60 rs (rs - 1) (2 rs - 1)
    if (r>=rcut) return 0.0;
    else if (r<= rcut_inner) return 0.0;
    double rs = r - rcut_inner;
    return 60.0 * rs * (rs - 1.0) * (2.0 * rs - 1.0);
}
double Cut_PolyS::calc_double_prime(double r) {
    // calc_prime(r) = h(rs) = -30 (rs-1)^2 rs^2,  rs = r - rcut_inner,
    //                                              drs/dr = 1
    // h(rs) = -30 (rs^4 - 2 rs^3 + rs^2)
    // h'(rs) = -120 rs^3 + 180 rs^2 - 60 rs = -60 rs (2 rs - 1)(rs - 1)
    // f''(r) = h'(rs) * drs/dr = -60 rs (rs - 1)(2 rs - 1)
    if (r>=rcut) return 0.0;
    else if (r<= rcut_inner) return 0.0;
    double rs = r - rcut_inner;
    return -60.0 * rs * (rs - 1.0) * (2.0 * rs - 1.0);
}

Cut_CosS::Cut_CosS() {}
Cut_CosS::Cut_CosS(double rcut)
{
    set_rcut(rcut);
    test_rcut(rcut);
}

std::string Cut_CosS::label() {
    return lab;
}

void Cut_CosS::set_rcut(const double r) {
    test_rcut(r);
    rcut=r;
    rcut_sq=r*r;
    rcut_inv= r<=0 ? 0.0 : 1.0/r;
    rcut_inner=rcut-1.0;
}
double Cut_CosS::get_rcut() {
    return rcut;
}

double Cut_CosS::get_rcut_sq() {
    return rcut_sq;
}

double Cut_CosS::calc(double r) {
  if (r>=rcut) return 0.0;
  else if (r<= rcut_inner) return 1.0;
  double rs = (r-rcut_inner)/(rcut-rcut_inner);
  return 0.5*(1+std::cos(M_PI*rs));
}
double Cut_CosS::calc_prime(double r) {
  if (r>=rcut || r<= rcut_inner) return 0.0;
  else {
    double rs = (r - rcut_inner) / (rcut - rcut_inner);
    double drs_dr = 1.0 / (rcut - rcut_inner);
    return -0.5 * M_PI * std::sin(M_PI * rs) * drs_dr;
  }
}
double Cut_CosS::calc_drcut(double r) {
  // rcut_inner = rcut - 1, so width (rcut - rcut_inner) = 1 is fixed.
  // rs = (r - rcut_inner) / 1 = r - rcut + 1; drs/drcut = -1.
  // df/drcut = -(pi/2) sin(pi*rs) * (-1) = (pi/2) sin(pi*rs)
  if (r>=rcut || r<= rcut_inner) return 0.0;
  double rs = (r - rcut_inner) / (rcut - rcut_inner);
  return 0.5 * M_PI * std::sin(M_PI * rs);
}
double Cut_CosS::calc_prime_drcut(double r) {
  // calc_prime(r) = -(pi/2) sin(pi rs),  rs = r - rcut + 1 (width fixed = 1)
  // drs/drcut = -1
  // d(calc_prime)/drcut = -(pi/2) cos(pi rs) * pi * drs/drcut
  //                     = (pi^2/2) cos(pi rs)
  if (r>=rcut || r<= rcut_inner) return 0.0;
  double rs = (r - rcut_inner) / (rcut - rcut_inner);
  return 0.5 * M_PI * M_PI * std::cos(M_PI * rs);
}
double Cut_CosS::calc_double_prime(double r) {
  // f(r)   = (1/2)(1 + cos(pi rs)),  rs = (r - rcut_inner)/width,
  //                                  width = rcut - rcut_inner = 1 (fixed)
  // drs/dr = 1
  // f'(r)  = -(pi/2) sin(pi rs)
  // f''(r) = -(pi^2/2) cos(pi rs)
  if (r>=rcut || r<= rcut_inner) return 0.0;
  double rs = (r - rcut_inner) / (rcut - rcut_inner);
  return -0.5 * M_PI * M_PI * std::cos(M_PI * rs);
}

Cut_PT::Cut_PT() {}
Cut_PT::Cut_PT(double rcut) {
  set_rcut(rcut);
  test_rcut(rcut);
}
std::string Cut_PT::label() {
  return lab;
}

// Set the outer cutoff radius
void Cut_PT::set_rcut(const double r) {
    test_rcut(r);
    rcut=r;
    rcut_sq=r*r;
    rcut_inv= r<=0 ? 0.0 : 1.0/r;
    rcut_inner=-rcut/2;
    double u0 = 0.5 * (rcut / (0.0 - rcut_inner) + rcut / (0.0 - rcut));
    norm = 1.0 / (0.5 + 0.5 * std::tanh(u0));
}
double Cut_PT::get_rcut() {
  return rcut;
}
double Cut_PT::get_rcut_sq() {
  return rcut_sq;
}
double Cut_PT::calc(double r) {
  if (r>=rcut) return 0.0;
  double u = 0.5 * ((rcut / (r - rcut_inner)) + (rcut / (r - rcut)));
  return (0.5 + 0.5 * tanh(u)) * norm;
}
double Cut_PT::calc_prime(double r) {
  if (r>=rcut) return 0.0;
  double u = 0.5 * ((rcut / (r - rcut_inner)) + (rcut / (r - rcut)));
  double cosh_u = cosh(u);
  double sech2_u = 1.0 / (cosh_u * cosh_u);

  double term1 = 1.0 / ((r - rcut_inner) * (r - rcut_inner));
  double term2 = 1.0 / ((r - rcut) * (r - rcut));
  double sum_terms = term1 + term2;

  return - (rcut / 4.0) * sech2_u * sum_terms * norm;
}
double Cut_PT::calc_drcut(double r) {
  // rcut_inner = -rcut/2, so r - rcut_inner = r + rcut/2.
  // norm is fixed by u(r=0) = 1/2 (independent of rcut), so dnorm/drcut = 0.
  // d(rcut/(r - rcut_inner))/drcut = r/(r - rcut_inner)^2  [r + rcut/2 grows with rcut]
  // d(rcut/(r - rcut))/drcut       = r/(r - rcut)^2
  // du/drcut = (r/2)*(1/(r - rcut_inner)^2 + 1/(r - rcut)^2)
  // df/drcut = (1/2) sech^2(u) * du/drcut * norm
  //          = (r/4) sech^2(u) * (1/(r - rcut_inner)^2 + 1/(r - rcut)^2) * norm
  if (r>=rcut) return 0.0;
  double u = 0.5 * ((rcut / (r - rcut_inner)) + (rcut / (r - rcut)));
  double cosh_u = std::cosh(u);
  double sech2_u = 1.0 / (cosh_u * cosh_u);
  double term1 = 1.0 / ((r - rcut_inner) * (r - rcut_inner));
  double term2 = 1.0 / ((r - rcut) * (r - rcut));
  return (r / 4.0) * sech2_u * (term1 + term2) * norm;
}
double Cut_PT::calc_prime_drcut(double r) {
  // f'(r) = -(rcut/4) * P * S * norm,
  //   P = sech^2(u),  S = 1/A^2 + 1/B^2,
  //   A = r - rcut_inner = r + rcut/2,   B = r - rcut.
  // norm independent of rcut; dnorm/drcut = 0.
  // dA/drcut = 1/2,  dB/drcut = -1.
  // dS/drcut = -2/A^3 * (1/2) + (-2/B^3) * (-1) = -1/A^3 + 2/B^3
  // u = 0.5 * (rcut/A + rcut/B);
  //   d(rcut/A)/drcut = (A - rcut * dA/drcut)/A^2 = (A - rcut/2)/A^2 = r/A^2
  //   d(rcut/B)/drcut = (B - rcut * dB/drcut)/B^2 = (B + rcut)/B^2 = r/B^2
  //   du/drcut = (r/2) * S
  // dP/du = -2 P tanh(u);  dP/drcut = -2 P tanh(u) * (r/2) S = -P r tanh(u) S
  // d(f')/drcut = -(1/4) P S norm
  //              + (-rcut/4) * dP/drcut * S * norm
  //              + (-rcut/4) * P * dS/drcut * norm
  //   = -(P norm/4) * [S - rcut * r * tanh(u) * S^2 + rcut * dS/drcut]
  if (r>=rcut) return 0.0;
  double A = r - rcut_inner;
  double B = r - rcut;
  double iA2 = 1.0/(A*A);
  double iB2 = 1.0/(B*B);
  double S = iA2 + iB2;
  double u = 0.5 * (rcut/A + rcut/B);
  double cosh_u = std::cosh(u);
  double sech2_u = 1.0/(cosh_u*cosh_u);
  double th_u = std::tanh(u);
  double dSdrcut = -1.0/(A*A*A) + 2.0/(B*B*B);
  return -0.25 * sech2_u * norm
         * (S - rcut * r * th_u * S * S + rcut * dSdrcut);
}
double Cut_PT::calc_double_prime(double r) {
  // f(r)  = (1/2 + 1/2 tanh(u)) * norm
  // f'(r) = (1/2) sech^2(u) * du/dr * norm
  //       = -(rcut/4) * P * S * norm,   P = sech^2(u),  S = 1/A^2 + 1/B^2
  //   A = r - rcut_inner = r + rcut/2,  B = r - rcut
  //   du/dr = -(rcut/2) S
  // f''(r) = -(rcut/4) * norm * d/dr[P * S]
  //   dP/du = -2 P tanh(u);   dP/dr = dP/du * du/dr = rcut P tanh(u) S
  //   dS/dr = -2/A^3 - 2/B^3
  //   d(P S)/dr = (dP/dr) S + P (dS/dr)
  //             = rcut P tanh(u) S^2 + P (-2/A^3 - 2/B^3)
  //             = P [rcut tanh(u) S^2 - 2/A^3 - 2/B^3]
  // f''(r) = -(rcut/4) * sech^2(u) * norm
  //          * [rcut * tanh(u) * S^2 - 2/A^3 - 2/B^3]
  if (r>=rcut) return 0.0;
  double A = r - rcut_inner;
  double B = r - rcut;
  double iA2 = 1.0/(A*A);
  double iB2 = 1.0/(B*B);
  double S = iA2 + iB2;
  double u = 0.5 * (rcut/A + rcut/B);
  double cosh_u = std::cosh(u);
  double sech2_u = 1.0/(cosh_u*cosh_u);
  double th_u = std::tanh(u);
  double inner = rcut * th_u * S * S - 2.0/(A*A*A) - 2.0/(B*B*B);
  return -0.25 * rcut * sech2_u * norm * inner;
}
Cut_Poly::Cut_Poly() {}
Cut_Poly::Cut_Poly(double rcut_value) {
  set_rcut(rcut_value);
}
std::string Cut_Poly::label() {
    return lab;
}
void Cut_Poly::set_rcut(const double r) {
    test_rcut(r);
    rcut = r;
    rcut_sq = r * r;
    rcut_inv= r<=0 ? 0.0 : 1.0/r;
}
double Cut_Poly::get_rcut() {
    return rcut;
}
double Cut_Poly::get_rcut_sq() {
    return rcut_sq;
}
double Cut_Poly::calc(double r) {
    if (r >= rcut) {
        return 0.0;
    } else {
        double x = r * rcut_inv; // x = r / rcut
        double x3 = x * x * x;
        double x4 = x3 * x;
        double x5 = x4 * x;
        return 1.0 - 10.0 * x3 + 15.0 * x4 - 6.0 * x5;
    }
}
double Cut_Poly::calc_prime(double r) {
    if (r >= rcut) {
        return 0.0;
    } else {
        double x = r * rcut_inv; // x = r / rcut
        double x2 = x * x;
        double x3 = x2 * x;
        double x4 = x3 * x;
        return (-30.0 * x2 + 60.0 * x3 - 30.0 * x4) * rcut_inv;
    }
}
double Cut_Poly::calc_drcut(double r) {
    // f = 1 - 10x^3 + 15x^4 - 6x^5,  x = r/rcut
    // df/dx = -30x^2 + 60x^3 - 30x^4;  dx/drcut = -r/rcut^2 = -x/rcut
    // df/drcut = (df/dx) * (-x/rcut) = (30x^3 - 60x^4 + 30x^5) / rcut
    if (r >= rcut) return 0.0;
    double x = r * rcut_inv;
    double x2 = x * x;
    double x3 = x2 * x;
    double x4 = x3 * x;
    double x5 = x4 * x;
    return (30.0 * x3 - 60.0 * x4 + 30.0 * x5) * rcut_inv;
}
double Cut_Poly::calc_prime_drcut(double r) {
    // calc_prime(r) = h(x)/rcut,  h(x) = -30 x^2 + 60 x^3 - 30 x^4
    // dx/drcut = -x/rcut
    // d(h(x)/rcut)/drcut
    //   = -h(x)/rcut^2 + (1/rcut) * h'(x) * dx/drcut
    //   = (1/rcut^2) * (-h(x) - x*h'(x))
    // h'(x) = -60 x + 180 x^2 - 120 x^3
    // -h(x) - x h'(x) = (30 x^2 - 60 x^3 + 30 x^4) - (-60 x^2 + 180 x^3 - 120 x^4)
    //                = 90 x^2 - 240 x^3 + 150 x^4
    if (r >= rcut) return 0.0;
    double x = r * rcut_inv;
    double x2 = x * x;
    double x3 = x2 * x;
    double x4 = x3 * x;
    return (90.0 * x2 - 240.0 * x3 + 150.0 * x4) * rcut_inv * rcut_inv;
}
double Cut_Poly::calc_double_prime(double r) {
    // f(r)   = 1 - 10 x^3 + 15 x^4 - 6 x^5,  x = r/rcut, dx/dr = 1/rcut
    // f'(r)  = (-30 x^2 + 60 x^3 - 30 x^4) / rcut
    // f''(r) = (-60 x + 180 x^2 - 120 x^3) / rcut^2
    //        = -60 x (1 - 3 x + 2 x^2) / rcut^2
    //        = -60 x (1 - x)(1 - 2 x) / rcut^2
    if (r >= rcut) return 0.0;
    double x = r * rcut_inv;
    double x2 = x * x;
    double x3 = x2 * x;
    return (-60.0 * x + 180.0 * x2 - 120.0 * x3) * rcut_inv * rcut_inv;
}

Cut_SinInv::Cut_SinInv() {}
Cut_SinInv::Cut_SinInv(double rc)
{
    set_rcut(rc);
    test_rcut(rc);
}
std::string Cut_SinInv::label() {
    return lab;
}
void Cut_SinInv::set_rcut(const double r) {
    test_rcut(r);
    rcut = r;
    rcut_sq = r*r;
    rcut_inv = r <= 0 ? 0.0 : 1.0/r;
}
double Cut_SinInv::get_rcut()    { return rcut; }
double Cut_SinInv::get_rcut_sq() { return rcut_sq; }
double Cut_SinInv::calc(double r) {
    if (r >= rcut) return 1.0;
    return std::sin(M_PI * r * rcut_inv * 0.5);
}
double Cut_SinInv::calc_prime(double r) {
    if (r >= rcut) return 0.0;
    double t = M_PI * rcut_inv * 0.5;
    return t * std::cos(t * r);
}
double Cut_SinInv::calc_drcut(double r) {
    // For r < rcut: f = sin(t r),  t = pi/(2 rcut),  dt/drcut = -t/rcut
    // df/drcut = cos(t r) * r * dt/drcut = -(r * t / rcut) * cos(t r)
    // For r >= rcut: f = 1 (constant), df/drcut = 0
    if (r >= rcut) return 0.0;
    double t = M_PI * rcut_inv * 0.5;
    return -r * t * rcut_inv * std::cos(t * r);
}
double Cut_SinInv::calc_prime_drcut(double r) {
    // For r < rcut: f'(r) = t cos(t r),  t = pi/(2 rcut)
    //   dt/drcut = -t/rcut,  d(t r)/drcut = -r t/rcut
    //   d(f')/drcut = (dt/drcut) cos(t r) + t (-sin(t r)) (d(t r)/drcut)
    //              = -t cos(t r)/rcut + (r t^2/rcut) sin(t r)
    //              = (t/rcut) (r t sin(t r) - cos(t r))
    // For r >= rcut: f' = 0 (constant), d(f')/drcut = 0
    if (r >= rcut) return 0.0;
    double t = M_PI * rcut_inv * 0.5;
    double tr = t * r;
    return t * rcut_inv * (r * t * std::sin(tr) - std::cos(tr));
}
double Cut_SinInv::calc_double_prime(double r) {
    // For r < rcut: f(r)  = sin(t r),     t = pi/(2 rcut)
    //               f'(r) = t cos(t r)
    //               f''(r) = -t^2 sin(t r)
    // For r >= rcut: f = 1 (constant), f'' = 0
    if (r >= rcut) return 0.0;
    double t = M_PI * rcut_inv * 0.5;
    return -t * t * std::sin(t * r);
}
double Cut_SinInv::calc(double r) const {
    if (r >= rcut) return 1.0;
    return std::sin(M_PI * r * rcut_inv * 0.5);
}
double Cut_SinInv::calc_prime(double r) const {
    if (r >= rcut) return 0.0;
    double t = M_PI * rcut_inv * 0.5;
    return t * std::cos(t * r);
}
double Cut_SinInv::calc_drcut(double r) const {
    // Mirrors the non-const overload; needed for F_mEnv<Cut_SinInv>::
    // calc_dF_dtheta (const) to chain the RCUTENV derivative through
    // a const fcut value member.
    if (r >= rcut) return 0.0;
    double t = M_PI * rcut_inv * 0.5;
    return -r * t * rcut_inv * std::cos(t * r);
}
double Cut_SinInv::calc_prime_drcut(double r) const {
    // Mirrors the non-const overload; needed for F_mEnv<Cut_SinInv>::
    // calc_dFp_dtheta (const) on the RCUTENV force-side chain.
    if (r >= rcut) return 0.0;
    double t  = M_PI * rcut_inv * 0.5;
    double tr = t * r;
    return t * rcut_inv * (r * t * std::sin(tr) - std::cos(tr));
}
double Cut_SinInv::calc_double_prime(double r) const {
    // Mirrors the non-const overload; needed for F_mEnv<Cut_SinInv>::
    // calc_d2F (const) on the radial-θ force chain.
    if (r >= rcut) return 0.0;
    double t = M_PI * rcut_inv * 0.5;
    return -t * t * std::sin(t * r);
}

} // namespace models
} // namespace tadah
