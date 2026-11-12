#include <tadah/models/descriptors/dm/dm_all.h>
#include <tadah/models/descriptors/density_reach.h>
#include <tadah/core/number_format.h>
#include <tadah/core/stats/data_stats.h>
#include <algorithm>
namespace tadah {
namespace models {

// ----------------------------------------------------------------------
// Specialisation of DM_mEAD<F>::meta() for the F_mEnv<Cut_SinInv>
// instantiation registered as "DM_mBlipEnv".
//
// This descriptor fails silently when its radial GTO reaches none of the
// data's neighbour distances (a narrow GTO, or a centre far from the
// neighbours): every density ρ collapses to zero and its design-matrix
// block is empty. The metadata captures every parameter that the
// descriptor reads from Context, with bound suggesters that read the seed
// and are anchored to dataset percentiles and KDE peaks, the reach check of
// the GTO (radial_reach_check) and a cross-parameter ρ-collapse health
// check.
// ----------------------------------------------------------------------
template<>
std::optional<DescriptorMeta>
DM_mEAD<F_mEnv<Cut_SinInv>>::meta() const {
  using DataStats = tadah::core::stats::DataStats;
  static constexpr std::size_t I_P1   = tadah::core::stats::IDX_P1;
  static constexpr std::size_t I_P10  = tadah::core::stats::IDX_P10;
  static constexpr std::size_t I_P90  = tadah::core::stats::IDX_P90;
  static constexpr std::size_t I_P99  = tadah::core::stats::IDX_P99;

  DescriptorMeta dm;
  dm.label = "DM_mBlipEnv";
  // Single source for this descriptor's DataStats rho-percentile key: the
  // closures below capture `self` instead of each hardcoding the label literal.
  const std::string self = dm.label;

  // RCUTMB — real-space cutoff for the MB neighbour sum.
  ParamMeta p_rcut;
  p_rcut.key      = "RCUTMB";
  p_rcut.arg_pos  = -1;
  p_rcut.scale    = Scale::Distance;
  p_rcut.units    = "Å";
  p_rcut.role     = "Real-space cutoff for the MB neighbour sum.";
  p_rcut.hard.lo  = 0.0;
  p_rcut.crit     = Criticality::Critical;
  p_rcut.scope    = PairScope::Global;
  p_rcut.suggest = cutoff_suggestion;
  p_rcut.reach_each_pair = true;
  p_rcut.check    = [](const DataStats& d, const SeedView& s) {
    return cutoff_reach_check(d, s, "RCUTMB");
  };
  dm.params.push_back(std::move(p_rcut));

  // CGRIDMB — radial GTO centre; a GTO that reaches no neighbour collapses ρ.
  ParamMeta p_cgrid;
  p_cgrid.key      = "CGRIDMB";
  p_cgrid.arg_pos  = -1;            // DM_mBlipEnv requires cgrid size 1
  p_cgrid.scale    = Scale::Distance;
  p_cgrid.units    = "Å";
  p_cgrid.role     = "Centre of the radial GTO exp(-SGRIDMB (r - CGRIDMB)^2). "
                     "A GTO below 1e-6 at every neighbour distance collapses ρ.";
  p_cgrid.hard.lo  = 0.0;
  p_cgrid.crit     = Criticality::Critical;
  p_cgrid.scope    = PairScope::PerElementPair;
  p_cgrid.suggest  = gaussian_centre_suggestion;
  p_cgrid.check    = [](const DataStats& d, const SeedView& s) {
    return radial_reach_check(d, s, BasisKind::Gaussian);
  };
  dm.params.push_back(std::move(p_cgrid));

  // SGRIDMB — radial GTO inverse width.
  ParamMeta p_sgrid;
  p_sgrid.key      = "SGRIDMB";
  p_sgrid.arg_pos  = -1;
  p_sgrid.scale    = Scale::InverseDistanceSq;
  p_sgrid.units    = "Å^-2";
  p_sgrid.role     = "Inverse width of the radial GTO. Narrow GTOs "
                     "combined with off-shell centres collapse ρ to 0.";
  p_sgrid.hard.lo  = kHardLoPositive;
  p_sgrid.crit     = Criticality::Critical;
  p_sgrid.scope    = PairScope::Global;
  p_sgrid.suggest  = gaussian_width_suggestion;
  dm.params.push_back(std::move(p_sgrid));

  // RCUTENV — inverse-cutoff envelope on ρ (Cut_SinInv).
  ParamMeta p_renv;
  p_renv.key      = "RCUTENV";
  p_renv.arg_pos  = -1;
  p_renv.scale    = Scale::Density;
  p_renv.units    = "ρ";
  p_renv.role     = "Inverse-cutoff envelope on ρ. The Cut_SinInv ramp "
                    "(0 at ρ=0, sin(πρ/(2 RCUTENV)) up to ρ=RCUTENV, "
                    "saturated to 1 above) should bracket the data's "
                    "active ρ band; otherwise the envelope is inert.";
  p_renv.hard.lo  = kHardLoPositive;
  p_renv.crit     = Criticality::Recommended;
  p_renv.scope    = PairScope::Global;
  p_renv.suggest  = [self](const DataStats& d, const SeedView&,
                            int) -> BoundSuggestion {
    auto it = d.rho_percentiles_per_descriptor.find(self);
    if (it == d.rho_percentiles_per_descriptor.end())
      return BoundSuggestion::fixed_range(
          0.5, 5.0, "fixed range: the statistics have no densities ρ");
    const auto& rho = it->second;
    return BoundSuggestion(rho[I_P10], rho[I_P90],
                           "the 10th to 90th percentiles of ρ");
  };
  dm.params.push_back(std::move(p_renv));

  // CEMBFUNC — vector: i-th embedding-blip centre in ρ-space.
  ParamMeta p_cemb;
  p_cemb.key      = "CEMBFUNC";
  p_cemb.arg_pos  = 0;
  p_cemb.scale    = Scale::Density;
  p_cemb.units    = "ρ";
  p_cemb.role     = "Centre of the i-th embedding blip in ρ, CEMBFUNC (i).";
  p_cemb.crit     = Criticality::Recommended;
  p_cemb.scope    = PairScope::Global;
  p_cemb.suggest  = [self](const DataStats& d, const SeedView&,
                            int) -> BoundSuggestion {
    auto it = d.rho_percentiles_per_descriptor.find(self);
    if (it == d.rho_percentiles_per_descriptor.end())
      return BoundSuggestion::fixed_range(
          1.0, 10.0, "fixed range: the statistics have no densities ρ");
    const auto& rho = it->second;
    return BoundSuggestion(rho[I_P10], rho[I_P90],
                           "the 10th to 90th percentiles of ρ");
  };
  dm.params.push_back(std::move(p_cemb));

  // SEMBFUNC — vector: i-th embedding-blip width η_p (1/ρ).
  ParamMeta p_semb;
  p_semb.key      = "SEMBFUNC";
  p_semb.arg_pos  = 0;
  p_semb.scale    = Scale::InverseDensity;
  p_semb.units    = "ρ^-1";
  p_semb.role     = "Inverse width η_p of the i-th embedding blip, "
                    "SEMBFUNC (i): its support is 4/η_p wide in ρ.";
  p_semb.hard.lo  = kHardLoPositive;
  p_semb.crit     = Criticality::Recommended;
  p_semb.scope    = PairScope::Global;
  p_semb.suggest  = [self](const DataStats& d, const SeedView& s,
                            int) -> BoundSuggestion {
    if (!s.contains("CEMBFUNC") || s.size("CEMBFUNC") < 2) {
      // Without two seed centres: from the spread of ρ, when known.
      auto it = d.rho_percentiles_per_descriptor.find(self);
      if (it != d.rho_percentiles_per_descriptor.end()) {
        const double w = it->second[I_P90] - it->second[I_P10];
        if (w > 0)
          return BoundSuggestion(0.5 / w, 8.0 / w,
                                 "0.5/w to 8/w with w the spread from the "
                                 "10th to the 90th percentile of ρ");
      }
      return BoundSuggestion::fixed_range(
          0.5, 4.0, "fixed range: the seed has fewer than two CEMBFUNC "
                    "centres and the statistics have no densities ρ");
    }
    auto v = s.vec("CEMBFUNC");
    const double mu_lo = *std::min_element(v.begin(), v.end());
    const double mu_hi = *std::max_element(v.begin(), v.end());
    const double n     = static_cast<double>(v.size());
    const double d_mu  = (mu_hi - mu_lo) / std::max(1.0, n - 1.0);
    if (!(d_mu > 0.0))
      return BoundSuggestion::fixed_range(
          0.5, 4.0, "fixed range: the seed's CEMBFUNC centres coincide");
    // Half-overlap (η ≈ 1/Δμ) anchored at the centre of the range: the
    // seed's centres, not the data, give this range.
    return BoundSuggestion::fixed_range(
        0.25 / d_mu, 4.0 / d_mu,
        "fixed range: 0.25/d to 4/d with d = " +
            tadah::core::format_roundtrip(d_mu) +
            " the spacing of the seed's CEMBFUNC centres, not the data");
  };
  dm.params.push_back(std::move(p_semb));

  // ----- Cross-parameter ρ-collapse check -----
  dm.cross_param_checks.push_back(
    [self](const DataStats& d, const SeedView&) -> HealthVerdict {
      auto it = d.rho_percentiles_per_descriptor.find(self);
      if (it == d.rho_percentiles_per_descriptor.end()) return HealthOk{};
      const auto& rho = it->second;
      if (rho[I_P99] < kDensityReachFloor) {
        return HealthFail{
          "ρ collapses to ≤ 1e-6 for 99% of atoms: the radial GTO does not "
          "reach the neighbour shells (a centre CGRIDMB nearer the "
          "neighbours, or a smaller SGRIDMB, reaches them)."};
      }
      const double denom = (rho[I_P1] > 0) ? rho[I_P1] : 1.0e-30;
      if (rho[I_P99] / denom < 1.05) {
        return HealthWarn{
          "ρ varies < 5% across the training pool — descriptor "
          "carries no information."};
      }
      return HealthOk{};
    });

  dm.expected_columns = [](const SeedView& s) -> std::size_t {
    if (!s.contains("CEMBFUNC")) return 0;
    return s.size("CEMBFUNC");
  };

  return dm;
}


template<>
tadah::core::Registry<DM_Base>::Map& tadah::core::Registry<DM_Base>::registry =
  tadah::core::Registry<tadah::models::DM_Base>::instance();
template<>
tadah::core::Registry<DM_Base,tadah::core::Context&>::Map& tadah::core::Registry<DM_Base,tadah::core::Context&>::registry =
  tadah::core::Registry<tadah::models::DM_Base,tadah::core::Context&>::instance();
template<>
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Map& tadah::core::Registry<DM_Base,const tadah::core::Context&>::registry =
  tadah::core::Registry<tadah::models::DM_Base,const tadah::core::Context&>::instance();

tadah::core::Registry<D_Base>::Register<DM_Blip> DM_Blip_0( "DM_Blip" );
tadah::core::Registry<D_Base>::Register<DM_Dummy> DM_Dummy_0( "DM_Dummy" );
tadah::core::Registry<D_Base>::Register<DM_EAD> DM_EAD_0( "DM_EAD" );
tadah::core::Registry<D_Base>::Register<DM_EAM> DM_EAM_0( "DM_EAM" );
tadah::core::Registry<D_Base>::Register<DM_REAM> DM_REAM_0( "DM_REAM" );
tadah::core::Registry<D_Base>::Register<DM_Embed> DM_Embed_0( "DM_Embed" );
tadah::core::Registry<D_Base>::Register<DM_mEAD<F_RLR>> DM_mEAD_0("DM_mRLR");
tadah::core::Registry<D_Base>::Register<DM_mEAD<F_SQRT>> DM_mEAD_00("DM_mSQRT");
tadah::core::Registry<D_Base>::Register<DM_mEAD<F_Blip>> DM_mEAD_000("DM_mBlip");
tadah::core::Registry<D_Base>::Register<DM_mEAD<F_mEnv<Cut_SinInv>>> DM_mEAD_0000("DM_mBlipEnv");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_SQ>>                    DM_PS_SQ_0   ("DM_PS_SQ");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_SQRT>>                  DM_PS_SQRT_0 ("DM_PS_SQRT");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_RLR>>                   DM_PS_RLR_0  ("DM_PS_RLR");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_Blip>>                  DM_PS_Blip_0 ("DM_PS_Blip");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_SQ,   GaussianRadial>>           DM_GPS_SQ_0  ("DM_GPS_SQ");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_SQRT, GaussianRadial>>           DM_GPS_SQRT_0("DM_GPS_SQRT");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_RLR,  GaussianRadial>>           DM_GPS_RLR_0 ("DM_GPS_RLR");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_Blip, GaussianRadial>>           DM_GPS_Blip_0("DM_GPS_Blip");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_SQ,   BlipRadial, OrbitalWise>>  DM_OW_SQ_0   ("DM_OW_SQ");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_SQRT, BlipRadial, OrbitalWise>>  DM_OW_SQRT_0 ("DM_OW_SQRT");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_RLR,  BlipRadial, OrbitalWise>>  DM_OW_RLR_0  ("DM_OW_RLR");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_Blip, BlipRadial, OrbitalWise>>  DM_OW_Blip_0 ("DM_OW_Blip");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_SQ,   GaussianRadial,OrbitalWise>> DM_GOW_SQ_0  ("DM_GOW_SQ");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_SQRT, GaussianRadial,OrbitalWise>> DM_GOW_SQRT_0("DM_GOW_SQRT");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_RLR,  GaussianRadial,OrbitalWise>> DM_GOW_RLR_0 ("DM_GOW_RLR");
tadah::core::Registry<D_Base>::Register<DM_Poly<F_Blip, GaussianRadial,OrbitalWise>> DM_GOW_Blip_0("DM_GOW_Blip");
tadah::core::Registry<D_Base>::Register<DM_mJoin> DM_mJoin_0("DM_mJoin");

tadah::core::Registry<DM_Base>::Register<DM_Blip> DM_Blip_1( "DM_Blip" );
tadah::core::Registry<DM_Base>::Register<DM_Dummy> DM_Dummy_1( "DM_Dummy" );
tadah::core::Registry<DM_Base>::Register<DM_EAD> DM_EAD_1( "DM_EAD" );
tadah::core::Registry<DM_Base>::Register<DM_EAM> DM_EAM_1( "DM_EAM" );
tadah::core::Registry<DM_Base>::Register<DM_REAM> DM_REAM_1( "DM_REAM" );
tadah::core::Registry<DM_Base>::Register<DM_Embed> DM_Embed_1( "DM_Embed" );
tadah::core::Registry<DM_Base>::Register<DM_mEAD<F_RLR>> DM_mEAD_1("DM_mRLR");
tadah::core::Registry<DM_Base>::Register<DM_mEAD<F_SQRT>> DM_mEAD_11("DM_mSQRT");
tadah::core::Registry<DM_Base>::Register<DM_mEAD<F_Blip>> DM_mEAD_111("DM_mBlip");
tadah::core::Registry<DM_Base>::Register<DM_mEAD<F_mEnv<Cut_SinInv>>> DM_mEAD_1111("DM_mBlipEnv");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_SQ>>                    DM_PS_SQ_1   ("DM_PS_SQ");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_SQRT>>                  DM_PS_SQRT_1 ("DM_PS_SQRT");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_RLR>>                   DM_PS_RLR_1  ("DM_PS_RLR");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_Blip>>                  DM_PS_Blip_1 ("DM_PS_Blip");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_SQ,   GaussianRadial>>           DM_GPS_SQ_1  ("DM_GPS_SQ");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_SQRT, GaussianRadial>>           DM_GPS_SQRT_1("DM_GPS_SQRT");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_RLR,  GaussianRadial>>           DM_GPS_RLR_1 ("DM_GPS_RLR");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_Blip, GaussianRadial>>           DM_GPS_Blip_1("DM_GPS_Blip");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_SQ,   BlipRadial, OrbitalWise>>  DM_OW_SQ_1   ("DM_OW_SQ");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_SQRT, BlipRadial, OrbitalWise>>  DM_OW_SQRT_1 ("DM_OW_SQRT");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_RLR,  BlipRadial, OrbitalWise>>  DM_OW_RLR_1  ("DM_OW_RLR");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_Blip, BlipRadial, OrbitalWise>>  DM_OW_Blip_1 ("DM_OW_Blip");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_SQ,   GaussianRadial,OrbitalWise>> DM_GOW_SQ_1  ("DM_GOW_SQ");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_SQRT, GaussianRadial,OrbitalWise>> DM_GOW_SQRT_1("DM_GOW_SQRT");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_RLR,  GaussianRadial,OrbitalWise>> DM_GOW_RLR_1 ("DM_GOW_RLR");
tadah::core::Registry<DM_Base>::Register<DM_Poly<F_Blip, GaussianRadial,OrbitalWise>> DM_GOW_Blip_1("DM_GOW_Blip");
tadah::core::Registry<DM_Base>::Register<DM_mJoin> DM_mJoin_1("DM_mJoin");

tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Blip> DM_Blip_2( "DM_Blip" );
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Dummy> DM_Dummy_2( "DM_Dummy" );
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_EAD> DM_EAD_2( "DM_EAD" );
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_EAM> DM_EAM_2( "DM_EAM" );
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_REAM> DM_REAM_2( "DM_REAM" );
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Embed> DM_Embed_2( "DM_Embed" );
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_mEAD<F_RLR>> DM_mEAD_2("DM_mRLR");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_mEAD<F_SQRT>> DM_mEAD_22("DM_mSQRT");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_mEAD<F_Blip>> DM_mEAD_222("DM_mBlip");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_mEAD<F_mEnv<Cut_SinInv>>> DM_mEAD_2222("DM_mBlipEnv");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_SQ>>                    DM_PS_SQ_2   ("DM_PS_SQ");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_SQRT>>                  DM_PS_SQRT_2 ("DM_PS_SQRT");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_RLR>>                   DM_PS_RLR_2  ("DM_PS_RLR");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_Blip>>                  DM_PS_Blip_2 ("DM_PS_Blip");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_SQ,   GaussianRadial>>           DM_GPS_SQ_2  ("DM_GPS_SQ");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_SQRT, GaussianRadial>>           DM_GPS_SQRT_2("DM_GPS_SQRT");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_RLR,  GaussianRadial>>           DM_GPS_RLR_2 ("DM_GPS_RLR");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_Blip, GaussianRadial>>           DM_GPS_Blip_2("DM_GPS_Blip");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_SQ,   BlipRadial, OrbitalWise>>  DM_OW_SQ_2   ("DM_OW_SQ");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_SQRT, BlipRadial, OrbitalWise>>  DM_OW_SQRT_2 ("DM_OW_SQRT");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_RLR,  BlipRadial, OrbitalWise>>  DM_OW_RLR_2  ("DM_OW_RLR");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_Blip, BlipRadial, OrbitalWise>>  DM_OW_Blip_2 ("DM_OW_Blip");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_SQ,   GaussianRadial,OrbitalWise>> DM_GOW_SQ_2  ("DM_GOW_SQ");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_SQRT, GaussianRadial,OrbitalWise>> DM_GOW_SQRT_2("DM_GOW_SQRT");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_RLR,  GaussianRadial,OrbitalWise>> DM_GOW_RLR_2 ("DM_GOW_RLR");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_Poly<F_Blip, GaussianRadial,OrbitalWise>> DM_GOW_Blip_2("DM_GOW_Blip");
tadah::core::Registry<DM_Base,tadah::core::Context&>::Register<DM_mJoin> DM_mJoin_2( "DM_mJoin" );

tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Blip> cDM_Blip_2( "DM_Blip" );
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Dummy> cDM_Dummy_2( "DM_Dummy" );
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_EAD> cDM_EAD_2( "DM_EAD" );
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_EAM> cDM_EAM_2( "DM_EAM" );
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_REAM> cDM_REAM_2( "DM_REAM" );
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Embed> cDM_Embed_2( "DM_Embed" );
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_mEAD<F_RLR>> cDM_mEAD_2("DM_mRLR");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_mEAD<F_SQRT>> cDM_mEAD_22("DM_mSQRT");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_mEAD<F_Blip>> cDM_mEAD_222("DM_mBlip");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_mEAD<F_mEnv<Cut_SinInv>>> cDM_mEAD_2222("DM_mBlipEnv");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_SQ>>                    cDM_PS_SQ_2   ("DM_PS_SQ");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_SQRT>>                  cDM_PS_SQRT_2 ("DM_PS_SQRT");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_RLR>>                   cDM_PS_RLR_2  ("DM_PS_RLR");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_Blip>>                  cDM_PS_Blip_2 ("DM_PS_Blip");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_SQ,   GaussianRadial>>           cDM_GPS_SQ_2  ("DM_GPS_SQ");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_SQRT, GaussianRadial>>           cDM_GPS_SQRT_2("DM_GPS_SQRT");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_RLR,  GaussianRadial>>           cDM_GPS_RLR_2 ("DM_GPS_RLR");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_Blip, GaussianRadial>>           cDM_GPS_Blip_2("DM_GPS_Blip");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_SQ,   BlipRadial, OrbitalWise>>  cDM_OW_SQ_2   ("DM_OW_SQ");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_SQRT, BlipRadial, OrbitalWise>>  cDM_OW_SQRT_2 ("DM_OW_SQRT");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_RLR,  BlipRadial, OrbitalWise>>  cDM_OW_RLR_2  ("DM_OW_RLR");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_Blip, BlipRadial, OrbitalWise>>  cDM_OW_Blip_2 ("DM_OW_Blip");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_SQ,   GaussianRadial,OrbitalWise>> cDM_GOW_SQ_2  ("DM_GOW_SQ");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_SQRT, GaussianRadial,OrbitalWise>> cDM_GOW_SQRT_2("DM_GOW_SQRT");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_RLR,  GaussianRadial,OrbitalWise>> cDM_GOW_RLR_2 ("DM_GOW_RLR");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_Poly<F_Blip, GaussianRadial,OrbitalWise>> cDM_GOW_Blip_2("DM_GOW_Blip");
tadah::core::Registry<DM_Base,const tadah::core::Context&>::Register<DM_mJoin> cDM_mJoin_2( "DM_mJoin" );
//Registry<DM_Base,tadah::core::Context&>::Register<DM_mEAD<F_RLR>> DM_mEAD_1( "DM_mEAD" );

} // namespace models
} // namespace tadah
