#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/models/functions/f_all.h>
#include <tadah/md/m_krr_md.h>

using namespace tadah::core;
using namespace tadah::models;
using namespace tadah::md;

TEST_CASE( "Testing M_KRR_MD Constructor 1", "[M_BLR_MD(kern, config)]" ) {
    Context c1 = CoreUserConfigParser().parse("tests_data/valid_configs/valid_config5");
    BF_Linear kern(c1);
    REQUIRE_THROWS(M_KRR_MD<>(kern, c1));
}
TEST_CASE( "Testing M_KRR_MD Constructor 2", "[M_BLR_MD(kern, config)]" ) {
    Context c1 = CoreUserConfigParser().parse("tests_data/valid_configs/valid_config5");
    Kern_Linear kern(c1);
    M_KRR_MD<> m(kern, c1);
}

TEST_CASE( "Testing M_KRR_MD Constructor 3", "[M_BLR_MD(config)]" ) {
    Context c1 = CoreUserConfigParser().parse("tests_data/valid_configs/valid_config5");
    M_KRR_MD<Kern_Linear> m(c1);
}
