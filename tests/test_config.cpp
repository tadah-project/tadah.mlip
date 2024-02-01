#include "catch2/catch.hpp"
#include "../CORE/config/config.h"
#include "../LIBS/Eigen/Core"
#include <filesystem>


TEST_CASE( "Testing Empty Config", "[config_empty]" ) {
    Config c1;
    Config c2;
    REQUIRE(c1==c2);
}

TEST_CASE( "Testing Config add()", "[config_add]" ) {
    Config c1;
    SECTION("Add internal") {
        c1.add("SIZE2B",10);
    }
    SECTION("Add invalid key name") {
        REQUIRE_THROWS(c1.add("SIZE2BX",10));
    }
    SECTION("Add invalid/empty key name") {
        REQUIRE_THROWS(c1.add("",10));
    }
    SECTION("Add the same key twice when not allowed case 1") {
        c1.add("SIZE2B",10);
        REQUIRE_THROWS(c1.add("SIZE2B",11));
    }
    SECTION("Add the same key twice when not allowed case 2") {
        c1.add("MODEL","M_BLR");
        c1.add("MODEL","BF_Linear");
        REQUIRE_THROWS(c1.add("MODEL","M_BLR"));
    }
    SECTION("Add allowed multiple keys") {
        c1.add("DBFILE", "f1");
        c1.add("DBFILE", "f2");
        c1.add("DBFILE", "f3");
    }
    SECTION("Add allowed negative multiple keys") {
        // Multiple negative allowed key
        c1.add("CGRID2B", 1);
        c1.add("CGRID2B", 2);
        c1.add("CGRID2B", 3);
        // Test various containers
        c1.add("CGRID2B", std::vector<double>{4,5,6});
        c1.add("CGRID2B", Eigen::Vector3d{7,8,9});
        REQUIRE(c1.size("CGRID2B")==9);
    }
}
TEST_CASE( "Testing Config remove()", "[config_remove]" ) {
    Config c1;
    // Single key allowed
    c1.add("SIZE2B",10);
    c1.add("SSTDEV",std::vector<double>{1,2,3,4,5,6});

    // Multiple allowed key
    c1.add("DBFILE", "f1");
    c1.add("DBFILE", "f2");
    c1.add("DBFILE", "f3");
  
    // Multiple negative allowed key
    c1.add("CGRID2B", 1);
    c1.add("CGRID2B", 2);
    c1.add("CGRID2B", 3);
    c1.add("CGRID2B", std::vector<double>{4,5,6});

    SECTION("Remove valid key name") {
        REQUIRE(c1.remove("SIZE2B")==1);
    }
    SECTION("Remove valid key name twice") {
        // init2b is set by default
        REQUIRE(c1.remove("INIT2B")==1);
        REQUIRE(c1.remove("INIT2B")==0);

        REQUIRE(c1.remove("SIZE2B")==1);
        REQUIRE(c1.remove("SIZE2B")==0);

        REQUIRE(c1.remove("SSTDEV")==1);
        REQUIRE(c1.remove("SSTDEV")==0);
    }
    SECTION("Remove multiple key name twice") {
        REQUIRE(c1.remove("DBFILE")==1);
        REQUIRE(c1.remove("DBFILE")==0);
    }
    SECTION("Remove multiple negative key name twice") {
        REQUIRE(c1.remove("CGRID2B")==1);
        REQUIRE(c1.remove("CGRID2B")==0);
    }
}
TEST_CASE( "Testing Config size()", "[config_size]" ) {
    Config c;
    REQUIRE(c.size("INIT2B")==1);
    REQUIRE(c.size("INIT3B")==1);
    REQUIRE(c.size("INITMB")==1);
    REQUIRE_THROWS(c.size("SIZE2B"));
    c.add("SIZE2B",12);
    REQUIRE(c.size("SIZE2B")==1);
    c.remove("SIZE2B");
    REQUIRE_THROWS(c.size("SIZE2B"));
}
TEST_CASE( "Testing Config exist()", "[config_exist]" ) {
    Config c;
    REQUIRE(c.exist("INIT2B"));
    REQUIRE(c.exist("INIT3B"));
    REQUIRE(c.exist("INITMB"));
}
TEST_CASE( "Testing Config clear_internal_keys()", "[config_clear_internal_keys]" ) {
    Config c1;
    Config c2;
    SECTION("Clear empty config") {
        c2.clear_internal_keys();
        REQUIRE(c1==c2);
    }
    SECTION("Clear modified config") {
        c2.add("DSIZE",10);
        c2.add("SIZE2B",4);
        c2.add("SIZE3B",3);
        c2.add("SIZEMB",3);
        c2.add("ATOMS",std::vector<std::string>{"Ti","Nb", "O"});
        c2.add("RCUTMAX",3.5);
        c2.add("ESTDEV",0.1444);
        c2.add("FSTDEV",0.1444);
        c2.add("SSTDEV",std::vector<double>{1,2,3,4,5,6});
        c2.clear_internal_keys();
        REQUIRE(c1==c2);
    }
}
TEST_CASE( "Testing Config operator()", "[config_operator()]" ) {
    std::string s1 = "f1/f1/f1";
    std::string s2 = "f2/f2/f2";
    std::string s3 = "f3/f3/f3";
    Config c;
    c.add("DBFILE",s1);
    c.add("DBFILE",s2);
    c.add("DBFILE",s3);
    auto v = c("DBFILE");
    REQUIRE(v[0]==s1);
    REQUIRE(v[1]==s2);
    REQUIRE(v[2]==s3);

    // Check some default values
    v = c("INIT2B");
    REQUIRE(v[0]=="false");
    v = c("INIT3B");
    REQUIRE(v[0]=="false");
    v = c("INITMB");
    REQUIRE(v[0]=="false");
}
TEST_CASE( "Testing is Config case sensitive", "[config_case]" ) {
    Config c;
    REQUIRE_THROWS(c.size("sIze2B"));
    c.add("SiZE2b",10);
    REQUIRE(c.exist("SIZE2B"));
    REQUIRE(c.exist("SIze2B"));
    REQUIRE(!(c.exist("SIze2Bx")));
    REQUIRE(!(c.exist(" SIze2B")));
    REQUIRE(c.size("sIze2B")==1);
    REQUIRE(c.remove("Size2b")==1);
}
TEST_CASE( "Testing Config get(k)", "[config_get]" ) {
    Config c;
    std::string s1 = "f1/f1/f1";
    std::string s2 = "f2/f2/f2";
    std::string s3 = "f3/f3/f3";
    c.add("DBFILE",std::vector<std::string>{s1,s2,s3});
    REQUIRE(s1==c.get<std::string>("DBFILE"));

    c.add("SSTDEV",std::vector<int>{1,2,3,4,5,6});
    REQUIRE(c.get<int>("SSTDEV")==1);
    REQUIRE(c.get<int>("SstdEV")==1);
}
TEST_CASE( "Testing Config get(k,v)", "[config_get_arr]" ) {
    Config c;
    std::string s1 = "f1/f1/f1";
    std::string s2 = "f2/f2/f2";
    std::string s3 = "f3/f3/f3";
    c.add("DBFILE",std::vector<std::string>{s1,s2,s3});
    std::vector<std::string> v(3);
    c.get("DBFILE",v);
    REQUIRE(v[0]==c("DBFILE")[0]);
    REQUIRE(v[1]==c("DBFILE")[1]);
    REQUIRE(v[2]==c("DBFILE")[2]);
    REQUIRE(v[2]==c("dbfilE")[2]);


    c.add("SSTDEV",std::vector<int>{1,2,3,4,5,6});
    std::vector<int> v2(6);
    c.get("SstDEV",v2);
    for (int i=0; i<6; ++i)
        REQUIRE(v2[i]==(i+1));
}
TEST_CASE( "Testing Config read()", "[config_read]" ) {
    for (auto const& valid_config :
            std::filesystem::directory_iterator("tests_data/valid_configs")) 
    {
        Config(valid_config.path().string());
    }
    for (auto const& invalid_config :
            std::filesystem::directory_iterator("tests_data/invalid_configs")) 
    {
        REQUIRE_THROWS(Config(invalid_config.path().string()));
    }

    Config c("tests_data/valid_configs/valid_config1");
    REQUIRE(c.size("DBFILE")==4);
}
