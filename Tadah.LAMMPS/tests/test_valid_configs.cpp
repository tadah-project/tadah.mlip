// The configuration fixtures under tests_data/valid_configs must be valid
// under the current key grammar: a descriptor they declare can be built,
// and its columns match the model's weights.
// valid_config5 (a trained D2_Blip + M_BLR BF_Linear configuration, used by
// the M_BLR_MD and M_KRR_MD constructor tests) declared a bare
// `TYPE2B D2_Blip`, without the two grid sizes and the element pairs that
// D2_Blip takes, so building its descriptors fails since 1.6.0; it also
// had no ATOM key, without which BIAS adds no intercept column (the bias
// is per species), while its weights include one.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/dc_selector.h>

#include <memory>

using namespace tadah::core;
using tadah::models::DC_Selector;

TEST_CASE("valid_config5 builds the two-body descriptor it declares",
          "[valid_configs]") {
    PeriodicTable::initialize();
    const Context c =
        CoreUserConfigParser().parse("tests_data/valid_configs/valid_config5");
    std::unique_ptr<DC_Selector> dcs;
    REQUIRE_NOTHROW(dcs.reset(new DC_Selector(c)));
    REQUIRE(dcs->d2b != nullptr);
    CHECK(dcs->d2b->label() == "D2_Blip");
    CHECK(dcs->d2b->size() == 4);
    /* The intercept of its one species and the four D2_Blip functions:
     * one weight, one mean and one standard deviation each. */
    CHECK(dcs->bias == 1);
    CHECK(dcs->descriptor_size() == c.size("WEIGHTS"));
    CHECK(dcs->descriptor_size() == c.size("NMEAN"));
    CHECK(dcs->descriptor_size() == c.size("NSTDEV"));
    /* `* *`: the descriptor applies to every element pair. */
    CHECK(dcs->d2b->is_init_for_atoms(73, 73));
    CHECK(dcs->d2b->is_init_for_atoms(1, 118));
}
