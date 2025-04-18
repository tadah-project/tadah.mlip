#include "catch2/catch.hpp"
#include <limits>
#include <stdexcept>
#include <string>

#include <tadah/core/registry.h>
#include <tadah/core/config.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d_all.h>
#include <tadah/models/functions/basis_functions/bf_all.h>
#include <tadah/models/functions/kernels/kern_all.h>
#include <tadah/mlip/models/m_all.h>
#include <tadah/mlip/structure_db.h>
#include <tadah/mlip/st_descriptors_db.h>
#include <tadah/mlip/nn_finder.h>
#include <tadah/mlip/design_matrix/functions/dm_function_base.h>


using BF=BF_Linear;
using K=Kern_Linear;
using D2=D2_Dummy;
using D3=D3_Dummy;
using DM=DM_Dummy;
using C2=Cut_Dummy;
using C3=Cut_Dummy;
using CM=Cut_Dummy;

TEST_CASE("TFM Testing Atom PeriodicTable Initialization") {
  PeriodicTable::initialize();
}

TEST_CASE( "Testing Factory: Models", "[factory_models]" ) {
  PeriodicTable::initialize();

  Config config;
  config.add("MPARAMS",1);
  config.add("MPARAMS",2);
  config.add("MPARAMS",3);
  // Config contains defaults only, we want to run prediction
  // should compile and run
  for (auto b:tadah::core::Registry<DM_Function_Base,Config&>::registry) {
    DM_Function_Base *fb = tadah::core::factory<DM_Function_Base,Config&>(b.first,config);
    for (auto c:tadah::core::Registry<M_Tadah_Base,DM_Function_Base&,Config&>::registry) {
      if (dynamic_cast<Kern_Base*>(fb) != nullptr && c.first=="M_KRR") {
        M_Tadah_Base *tdb=nullptr;
        REQUIRE((tdb=tadah::core::factory<M_Tadah_Base,DM_Function_Base&,Config&>(c.first,*fb,config)));
        if (tdb) delete tdb;
      }
      else if (dynamic_cast<BF_Base*>(fb) != nullptr && c.first=="M_BLR") {
        M_Tadah_Base *tdb=nullptr;
        REQUIRE((tdb=tadah::core::factory<M_Tadah_Base,DM_Function_Base&,Config&>(c.first,*fb,config)));
        if (tdb) delete tdb;
      }
      else {
        M_Tadah_Base *tdb=nullptr;
        REQUIRE_THROWS_AS((tdb=tadah::core::factory<M_Tadah_Base,DM_Function_Base&,Config&>(c.first,*fb,config)),
                          std::invalid_argument);
        if (tdb) delete tdb;
      }
    }
    if (fb) delete fb;
  }

  //config = Config("datasets/config");
  //StructureDB stdb(config);
  //NNFinder nnf(config);
  //nnf.calc(stdb);
  //D2 d2b(config);
  //D3 d3b(config);
  //DM dmb(config);

  //Cut_Base *c2b;
  //Cut_Base *c3b;
  //Cut_Base *cmb;
  //double rcut2b = config.get<double>("RCUT2B");
  //double rcut3b = config.get<double>("RCUT3B");
  //double rcutmb = config.get<double>("RCUTMB");
  //for (auto c:Registry<Cut_Base,double>::registry) {
  //    c2b = factory<Cut_Base,double>( c.first, rcut2b );
  //    c3b = factory<Cut_Base,double>( c.first, rcut3b );
  //    cmb = factory<Cut_Base,double>( c.first, rcutmb );
  //}
  //DM_Function_Base *fb = factory<DM_Function_Base,Config&>(config.get<std::string>("MODEL",1),config);
  //for (auto m:Registry<M_Tadah_Base,DM_Function_Base,D2_Base,D3_Base,Base,Cut_Base,Cut_Base,Cut_Base,Config,StructureDB>::registry) {
  //}
  //M_Tadah_Base *model = factory<M_Tadah_Base,DM_Function_Base,D2_Base,D3_Base,Base,
  //      Cut_Base,Cut_Base,Cut_Base,Config,StructureDB>
  //          (config.get<std::string>("MODEL",0),bf,d2b,d3b,dmb,
  //           c2b,c3b,cmb,config,stdb);

}

TEST_CASE( "Testing M_BLR", "[m_blr]" ) {
  PeriodicTable::initialize();

  Config config;
  config.add("ATOM", "H");
  config.add("WATOM", 1);
  // Config contains defaults only, we run prediction
  // should throw runtime b/c key is not found
  //using M2=M_BLR<BF>;
  //M1 m1(config);
  //REQUIRE_THROWS_AS(M1(config),std::runtime_error);
  //REQUIRE_THROWS_WITH(M1(config), Catch::Matchers::Contains( "Key not found" ));
  // for later...
  D2 d2(config);
  D3 d3(config);
  DM dm(config);
  C2 c2(0.0);
  C3 c3(0.0);
  CM cm(0.0);

}

TEST_CASE( "Testing M_KRR", "[m_krr]" ) {
  PeriodicTable::initialize();

  Config config;
  config.add("ATOM", "H");
  config.add("WATOM", 1);
  // Config contains defaults only, we want to run prediction
  // should throw runtime b/c e.g. WEIGHTS key is not found
  //using M2=M_KRR<K>;
  //REQUIRE_THROWS_AS(M2(config),std::runtime_error);
  //REQUIRE_THROWS_WITH(M2(config), Catch::Matchers::Contains( "Key not found" ));
  // for later...
  D2 d2(config);
  D3 d3(config);
  DM dm(config);
  C2 c2(0.0);
  C3 c3(0.0);
  CM cm(0.0);

}

