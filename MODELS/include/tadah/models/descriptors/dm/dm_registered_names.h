#ifndef DM_REGISTERED_NAMES_H
#define DM_REGISTERED_NAMES_H

/* The names under which the descriptor registry (src/dm_all.cpp) registers
 * the instances of the descriptor templates DM_mEAD<F> and
 * DM_Poly<ChannelFn, RadialBasis, AggMode>: the TYPEMB value that names
 * each, and the label() it gives (messages, the HEALTH_LOG columns of
 * tadah hpo). An instance registered under no name (DM_mEAD<F_SQ>, a test
 * instance) keeps the family name, DM_mEAD or DM_Poly. A test builds every
 * registered descriptor through the registry and compares its label with
 * its registered name, so a registration and its name here cannot differ
 * unnoticed. */

namespace tadah {
namespace models {

class F_RLR;
class F_SQRT;
class F_SQ;
class F_Blip;
template <typename F> class F_mEnv;
class Cut_SinInv;
struct BlipRadial;
struct GaussianRadial;
struct PowerSpectrum;
struct OrbitalWise;

/** The registered name of DM_mEAD<F>; "DM_mEAD" for an F registered under
 *  no name. */
template <typename F> struct DM_mEAD_name {
  static constexpr const char *value = "DM_mEAD";
};
template <> struct DM_mEAD_name<F_RLR> {
  static constexpr const char *value = "DM_mRLR";
};
template <> struct DM_mEAD_name<F_SQRT> {
  static constexpr const char *value = "DM_mSQRT";
};
template <> struct DM_mEAD_name<F_Blip> {
  static constexpr const char *value = "DM_mBlip";
};
template <> struct DM_mEAD_name<F_mEnv<Cut_SinInv>> {
  static constexpr const char *value = "DM_mBlipEnv";
};

/** The registered name of DM_Poly<ChannelFn, RadialBasis, AggMode>;
 *  "DM_Poly" for an instance registered under no name. */
template <typename ChannelFn, typename RadialBasis, typename AggMode>
struct DM_Poly_name {
  static constexpr const char *value = "DM_Poly";
};

#define TADAH_DM_POLY_NAME(F, R, A, NAME)              \
  template <> struct DM_Poly_name<F, R, A> {           \
    static constexpr const char *value = NAME;         \
  };

TADAH_DM_POLY_NAME(F_SQ, BlipRadial, PowerSpectrum, "DM_PS_SQ")
TADAH_DM_POLY_NAME(F_SQRT, BlipRadial, PowerSpectrum, "DM_PS_SQRT")
TADAH_DM_POLY_NAME(F_RLR, BlipRadial, PowerSpectrum, "DM_PS_RLR")
TADAH_DM_POLY_NAME(F_Blip, BlipRadial, PowerSpectrum, "DM_PS_Blip")
TADAH_DM_POLY_NAME(F_SQ, GaussianRadial, PowerSpectrum, "DM_GPS_SQ")
TADAH_DM_POLY_NAME(F_SQRT, GaussianRadial, PowerSpectrum, "DM_GPS_SQRT")
TADAH_DM_POLY_NAME(F_RLR, GaussianRadial, PowerSpectrum, "DM_GPS_RLR")
TADAH_DM_POLY_NAME(F_Blip, GaussianRadial, PowerSpectrum, "DM_GPS_Blip")
TADAH_DM_POLY_NAME(F_SQ, BlipRadial, OrbitalWise, "DM_OW_SQ")
TADAH_DM_POLY_NAME(F_SQRT, BlipRadial, OrbitalWise, "DM_OW_SQRT")
TADAH_DM_POLY_NAME(F_RLR, BlipRadial, OrbitalWise, "DM_OW_RLR")
TADAH_DM_POLY_NAME(F_Blip, BlipRadial, OrbitalWise, "DM_OW_Blip")
TADAH_DM_POLY_NAME(F_SQ, GaussianRadial, OrbitalWise, "DM_GOW_SQ")
TADAH_DM_POLY_NAME(F_SQRT, GaussianRadial, OrbitalWise, "DM_GOW_SQRT")
TADAH_DM_POLY_NAME(F_RLR, GaussianRadial, OrbitalWise, "DM_GOW_RLR")
TADAH_DM_POLY_NAME(F_Blip, GaussianRadial, OrbitalWise, "DM_GOW_Blip")

#undef TADAH_DM_POLY_NAME

}  // namespace models
}  // namespace tadah

#endif  // DM_REGISTERED_NAMES_H
