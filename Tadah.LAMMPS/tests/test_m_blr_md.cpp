#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/models/functions/f_all.h>
#include <tadah/md/m_blr_md.h>

using namespace tadah::core;
using namespace tadah::models;
using namespace tadah::md;

TEST_CASE( "Testing M_BLR_MD Constructor 1", "[M_BLR_MD(bf, config)]" ) {
    Context c1 = CoreUserConfigParser().parse("tests_data/valid_configs/valid_config5");
    BF_Linear bf(c1);
    M_BLR_MD<> m(bf, c1);
}
TEST_CASE( "Testing M_BLR_MD Constructor 2", "[M_BLR_MD(bf, config)]" ) {
    Context c1 = CoreUserConfigParser().parse("tests_data/valid_configs/valid_config5");
    Kern_Linear bf(c1);
    REQUIRE_THROWS(M_BLR_MD<>(bf, c1));
}

TEST_CASE( "Testing M_BLR_MD Constructor 3", "[M_BLR_MD(config)]" ) {
    Context c1 = CoreUserConfigParser().parse("tests_data/valid_configs/valid_config5");
    M_BLR_MD<BF_Linear> m(c1);
}
