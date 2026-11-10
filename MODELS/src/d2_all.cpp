#include "tadah/models/descriptors/d2/d2_all.h"
namespace tadah {
namespace models {

template<>
tadah::core::Registry<D_Base>::Map& tadah::core::Registry<D_Base>::registry =
  tadah::core::Registry<tadah::models::D_Base>::instance();
template<>
tadah::core::Registry<D_Base,tadah::core::Context&>::Map& tadah::core::Registry<D_Base,tadah::core::Context&>::registry =
  tadah::core::Registry<tadah::models::D_Base,tadah::core::Context&>::instance();
template<>
tadah::core::Registry<D_Base,const tadah::core::Context&>::Map& tadah::core::Registry<D_Base,const tadah::core::Context&>::registry =
  tadah::core::Registry<tadah::models::D_Base,const tadah::core::Context&>::instance();
template<>
tadah::core::Registry<D2_Base,tadah::core::Context&>::Map& tadah::core::Registry<D2_Base,tadah::core::Context&>::registry =
  tadah::core::Registry<tadah::models::D2_Base,tadah::core::Context&>::instance();
template<>
tadah::core::Registry<D2_Base,const tadah::core::Context&>::Map& tadah::core::Registry<D2_Base,const tadah::core::Context&>::registry =
  tadah::core::Registry<tadah::models::D2_Base,const tadah::core::Context&>::instance();

tadah::core::Registry<D_Base>::Register<D2_Blip> D2_Blip_0( "D2_Blip" );
tadah::core::Registry<D_Base>::Register<D2_Knot5> D2_Knot5_0( "D2_Knot5" );
tadah::core::Registry<D_Base>::Register<D2_Blip5> D2_Blip5_0( "D2_Blip5" );
tadah::core::Registry<D_Base>::Register<D2_BP> D2_BP_0( "D2_BP" );
tadah::core::Registry<D_Base>::Register<D2_Dummy> D2_Dummy_0( "D2_Dummy" );
tadah::core::Registry<D_Base>::Register<D2_EAM> D2_EAM_0( "D2_EAM" );
tadah::core::Registry<D_Base>::Register<D2_LJ> D2_LJ_0( "D2_LJ" );
tadah::core::Registry<D_Base>::Register<D2_MIE> D2_MIE_0( "D2_MIE" );
tadah::core::Registry<D_Base>::Register<D2_ZBL> D2_ZBL_0( "D2_ZBL" );
tadah::core::Registry<D_Base>::Register<D2_mJoin> D2_mJoin_0( "D2_mJoin" );

tadah::core::Registry<D_Base,tadah::core::Context&>::Register<D2_Blip> D2_Blip_00( "D2_Blip" );
tadah::core::Registry<D_Base,tadah::core::Context&>::Register<D2_Knot5> D2_Knot5_00( "D2_Knot5" );
tadah::core::Registry<D_Base,tadah::core::Context&>::Register<D2_Blip5> D2_Blip5_00( "D2_Blip5" );
tadah::core::Registry<D_Base,tadah::core::Context&>::Register<D2_BP> D2_BP_00( "D2_BP" );
tadah::core::Registry<D_Base,tadah::core::Context&>::Register<D2_Dummy> D2_Dummy_00( "D2_Dummy" );
tadah::core::Registry<D_Base,tadah::core::Context&>::Register<D2_EAM> D2_EAM_00( "D2_EAM" );
tadah::core::Registry<D_Base,tadah::core::Context&>::Register<D2_LJ> D2_LJ_00( "D2_LJ" );
tadah::core::Registry<D_Base,tadah::core::Context&>::Register<D2_MIE> D2_MIE_00( "D2_MIE" );
tadah::core::Registry<D_Base,tadah::core::Context&>::Register<D2_ZBL> D2_ZBL_00( "D2_ZBL" );
tadah::core::Registry<D_Base,tadah::core::Context&>::Register<D2_mJoin> D2_mJoin_00( "D2_mJoin" );

tadah::core::Registry<D_Base,const tadah::core::Context&>::Register<D2_Blip> cD2_Blip_00( "D2_Blip" );
tadah::core::Registry<D_Base,const tadah::core::Context&>::Register<D2_Knot5> cD2_Knot5_00( "D2_Knot5" );
tadah::core::Registry<D_Base,const tadah::core::Context&>::Register<D2_Blip5> cD2_Blip5_00( "D2_Blip5" );
tadah::core::Registry<D_Base,const tadah::core::Context&>::Register<D2_BP> cD2_BP_00( "D2_BP" );
tadah::core::Registry<D_Base,const tadah::core::Context&>::Register<D2_Dummy> cD2_Dummy_00( "D2_Dummy" );
tadah::core::Registry<D_Base,const tadah::core::Context&>::Register<D2_EAM> cD2_EAM_00( "D2_EAM" );
tadah::core::Registry<D_Base,const tadah::core::Context&>::Register<D2_LJ> cD2_LJ_00( "D2_LJ" );
tadah::core::Registry<D_Base,const tadah::core::Context&>::Register<D2_MIE> cD2_MIE_00( "D2_MIE" );
tadah::core::Registry<D_Base,const tadah::core::Context&>::Register<D2_ZBL> cD2_ZBL_00( "D2_ZBL" );
tadah::core::Registry<D_Base,const tadah::core::Context&>::Register<D2_mJoin> cD2_mJoin_00( "D2_mJoin" );

tadah::core::Registry<D2_Base,tadah::core::Context&>::Register<D2_Blip> D2_Blip_1( "D2_Blip" );
tadah::core::Registry<D2_Base,tadah::core::Context&>::Register<D2_Knot5> D2_Knot5_1( "D2_Knot5" );
tadah::core::Registry<D2_Base,tadah::core::Context&>::Register<D2_Blip5> D2_Blip5_1( "D2_Blip5" );
tadah::core::Registry<D2_Base,tadah::core::Context&>::Register<D2_BP> D2_BP_1( "D2_BP" );
tadah::core::Registry<D2_Base,tadah::core::Context&>::Register<D2_Dummy> D2_Dummy_1( "D2_Dummy" );
tadah::core::Registry<D2_Base,tadah::core::Context&>::Register<D2_EAM> D2_EAM_1( "D2_EAM" );
tadah::core::Registry<D2_Base,tadah::core::Context&>::Register<D2_LJ> D2_LJ_1( "D2_LJ" );
tadah::core::Registry<D2_Base,tadah::core::Context&>::Register<D2_MIE> D2_MIE_1( "D2_MIE" );
tadah::core::Registry<D2_Base,tadah::core::Context&>::Register<D2_ZBL> D2_ZBL_1( "D2_ZBL" );
tadah::core::Registry<D2_Base,tadah::core::Context&>::Register<D2_mJoin> D2_mJoin_1( "D2_mJoin" );

tadah::core::Registry<D2_Base,const tadah::core::Context&>::Register<D2_Blip> cD2_Blip_1( "D2_Blip" );
tadah::core::Registry<D2_Base,const tadah::core::Context&>::Register<D2_Knot5> cD2_Knot5_1( "D2_Knot5" );
tadah::core::Registry<D2_Base,const tadah::core::Context&>::Register<D2_Blip5> cD2_Blip5_1( "D2_Blip5" );
tadah::core::Registry<D2_Base,const tadah::core::Context&>::Register<D2_BP> cD2_BP_1( "D2_BP" );
tadah::core::Registry<D2_Base,const tadah::core::Context&>::Register<D2_Dummy> cD2_Dummy_1( "D2_Dummy" );
tadah::core::Registry<D2_Base,const tadah::core::Context&>::Register<D2_EAM> cD2_EAM_1( "D2_EAM" );
tadah::core::Registry<D2_Base,const tadah::core::Context&>::Register<D2_LJ> cD2_LJ_1( "D2_LJ" );
tadah::core::Registry<D2_Base,const tadah::core::Context&>::Register<D2_MIE> cD2_MIE_1( "D2_MIE" );
tadah::core::Registry<D2_Base,const tadah::core::Context&>::Register<D2_ZBL> cD2_ZBL_1( "D2_ZBL" );
tadah::core::Registry<D2_Base,const tadah::core::Context&>::Register<D2_mJoin> cD2_mJoin_1( "D2_mJoin" );

} // namespace models
} // namespace tadah
