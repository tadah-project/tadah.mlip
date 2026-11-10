/* M_Core::reset_trained() must clear the trained flag.
 *
 * M_Core::trained is set to true after the first model::train() call. A
 * model trained again in place must be reset before training, or the
 * training call throws "This object is already trained!". reset_trained()
 * is the way to retrain a model, and this test ensures the flag is cleared
 * correctly. */

#include "catch2/catch.hpp"
#include <tadah/core/core_types.h>
#include <tadah/models/m_core.h>

namespace {

// Minimal concrete M_Core subclass used only by the test below.
struct ConcreteModel : tadah::models::M_Core {
    double predict(const tadah::core::aed_type&) const override { return 0.0; }
};

} // anonymous namespace

TEST_CASE("M_Core::reset_trained clears the trained flag", "[model]")
{
    ConcreteModel m;
    REQUIRE(m.is_trained() == false);   // default: not yet trained

    m.trained = true;
    REQUIRE(m.is_trained() == true);

    m.reset_trained();
    REQUIRE(m.is_trained() == false);   // regression: must clear the flag
}
