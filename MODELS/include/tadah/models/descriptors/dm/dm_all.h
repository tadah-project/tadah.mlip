
#include <tadah/models/descriptors/dm/dm_base.h>
#include <tadah/models/descriptors/dm/dm_dummy.h>
#include <tadah/models/descriptors/dm/dm_eam.h>
#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/dm/dm_embed.h>
#include <tadah/models/descriptors/dm/dm_ead.h>
#include <tadah/models/descriptors/dm/dm_blip.h>
#include <tadah/models/descriptors/dm/dm_mEAD.h>
#include <tadah/models/descriptors/dm/dm_poly.h>
#include <tadah/models/descriptors/dm/dm_mjoin.h>

#include <tadah/models/descriptors/dm/dm_func.h>

namespace tadah {
namespace models {

// Forward-declare the explicit specialisation of DM_mEAD::meta() for
// the F_mEnv<Cut_SinInv> instantiation registered as "DM_mBlipEnv".
// This prevents implicit instantiation of the (nullopt) default in
// other TUs and guarantees the override is the one linked.
template<>
std::optional<DescriptorMeta>
DM_mEAD<F_mEnv<Cut_SinInv>>::meta() const;

} // namespace models
} // namespace tadah

