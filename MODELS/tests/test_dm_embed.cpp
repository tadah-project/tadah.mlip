#include "catch2/catch.hpp"
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/descriptors/dm/dm_embed.h>
#include <memory>
#include <sstream>

using namespace tadah::core;
using namespace tadah::models;
namespace {
Context embed_config(std::string density = "Blip5", std::string embedding = "Gaussian",
                     std::string form = "NONNEG", std::string cutoff = "Cut_Poly",
                     std::string envelope = "OFF", bool widths = false) {
  PeriodicTable::initialize();
  const bool dk = density == "Knot5", ek = embedding == "Knot5";
  std::string text = "VERBOSE 0\nINIT2B false\nINITMB true\nATOM Ta\n"
      "MODEL M_BLR BF_Linear\nDESCRIPTOR ta DM_Embed\nPAIRS Ta Ta\n"
      "DENSITY\nBASIS " + density + "\nFORM " + form + "\nCENTRES 1 2\n" +
      (dk ? "DEGREES 3 5\n" : widths ? "WIDTHS 0.5 0.5\n" : "ETA 2 2\n") +
      "AMPLITUDES 0.4 0.2\nCUTOFF\nTYPE " + cutoff +
      "\nRADIUS 6\nENDCUTOFF\nENDDENSITY\nEMBEDDING\nBASIS " + embedding +
      "\nCENTRES 0 0.5 1\n" + (ek ? "DEGREES 5 3 5\n" :
      widths ? "WIDTHS 0.5 0.5 0.5\n" : "ETA 2 2 2\n") +
      "REFERENCE ZERO\nENVELOPE\nTYPE " + envelope + "\n" +
      (envelope == "OFF" ? "" : "CUTOFF 2\n") +
      "ENDENVELOPE\nENDEMBEDDING\nENDDESCRIPTOR\n";
  std::istringstream in(text);
  return CoreUserConfigParser().parse(in);
}
double energy(DM_Base &d, double r) {
  rho_type rho; d.init_rhoi(rho);
  d.calc_rho(73,73,r,r*r,Vec3d(r,0,0),rho);
  aed_type a(d.size()); a.set_zero(); d.calc_aed(rho,a);
  double e=0; for(std::size_t i=0;i<d.size();++i) e+=(i+1)*a(i);
  return e;
}
}

TEST_CASE("DM_Embed constructs through the descriptor selector", "[unified-dm]") {
  auto c = embed_config();
  DC_Selector s(c);
  REQUIRE(s.dmb->label() == "DM_Embed");
  REQUIRE(s.dmb->size() == 3);
  REQUIRE(s.dmb->eam_form());
  REQUIRE(s.dmb->get_rcut() == 6.0);
  rho_type rho; s.dmb->init_rhoi(rho);
  aed_type a(3); a.set_zero(); s.dmb->calc_aed(rho,a);
  for(std::size_t m=0;m<3;++m) REQUIRE(a(m) == 0.0);
}

TEST_CASE("DM_Embed independently selects density embedding cutoff and envelope",
          "[unified-dm][matrix]") {
  const std::vector<std::string> families{
    "Blip","Blip5","Gaussian","Knot5","WendlandC2","WendlandC4"};
  for(const auto &d:families) for(const auto &e:families)
    for(const auto &cut:{"Cut_Cos","Cut_CosS","Cut_Poly","Cut_PolyS","Cut_Tanh","Cut_PT","NONE"})
      for(const auto &env:{"OFF","SinInv","Smoothstep2","Smoothstep4"}) {
        CAPTURE(d,e,cut,env);
        auto c=embed_config(d,e,"NONNEG",cut,env);
        if(d=="Gaussian" && std::string(cut)=="NONE") {
          REQUIRE_THROWS(DC_Selector(c)); continue;
        }
        DC_Selector s(c);
        const double r=1.37,h=1e-5;
        const double fd=(energy(*s.dmb,r+h)-energy(*s.dmb,r-h))/(2*h);
        rho_type rho; s.dmb->init_rhoi(rho);
        s.dmb->calc_rho(73,73,r,r*r,Vec3d(r,0,0),rho);
        aed_type a(3); a.set_zero(); s.dmb->calc_aed(rho,a);
        fd_type f(3); f.set_zero();
        s.dmb->calc_dXijdri(73,73,r,r*r,Vec3d(r,0,0),rho,f);
        double value=0; for(std::size_t m=0;m<3;++m) value+=(m+1)*f(m,0);
        REQUIRE(value == Approx(fd).margin(2e-7));
        t_type weights(3);double expected=0,expected_prime=0;
        for(std::size_t m=0;m<3;++m) {weights(m)=m+1;expected+=(m+1)*a(m);expected_prime+=(m+1)*rho(1+m);}
        double collapsed,prime;s.dmb->eam_embed(weights,rho(0),collapsed,prime);
        REQUIRE(collapsed==Approx(expected).margin(1e-13));REQUIRE(prime==Approx(expected_prime).margin(1e-13));
      }
}

TEST_CASE("DM_Embed force buffers are additive and an invalid component is refused",
          "[unified-dm]") {
  auto c=embed_config(); DC_Selector s(c);
  const double before=energy(*s.dmb,1.3);
  auto bad=c;
  auto blocks=bad.blocks();
  for(auto &line:blocks[0].lines) if(line.find("AMPLITUDES")!=std::string::npos)
    line="AMPLITUDES -1 1";
  bad.replace_blocks(blocks);
  // The constructor refuses the invalid component.
  REQUIRE_THROWS(DC_Selector(bad));
  REQUIRE(energy(*s.dmb,1.3) == before);
  rho_type rho; s.dmb->init_rhoi(rho);
  s.dmb->calc_rho(73,73,1.3,1.69,Vec3d(1.3,0,0),rho);
  aed_type a(3); a.set_zero(); s.dmb->calc_aed(rho,a);
  fd_type f(3),g(3); f.set_zero(); g.set_zero();
  s.dmb->calc_dXijdri(73,73,1.3,1.69,Vec3d(1.3,0,0),rho,f);
  for(std::size_t m=0;m<3;++m) g(m,0)=7;
  s.dmb->calc_dXijdri(73,73,1.3,1.69,Vec3d(1.3,0,0),rho,g);
  for(std::size_t m=0;m<3;++m) REQUIRE(g(m,0)==Approx(7+f(m,0)));
}

TEST_CASE("DM_Embed Wendland values derivatives and integrated tails have independent rational references",
          "[unified-dm][reference]") {
  for(bool c4:{false,true}) {
    auto c=embed_config(c4?"WendlandC4":"WendlandC2",c4?"WendlandC4":"WendlandC2","MONOTONE","NONE");
    auto spec=embed_specs(c).at(0); EmbedEvaluator e(spec);
    const auto g=e.embedding(0,0.25);
    // x = eta*q = 1/2. Independently expanded/differentiated polynomials.
    REQUIRE(g.value==Approx((c4?83.0/768:3.0/16)-1));
    REQUIRE(g.deriv==Approx(2*(c4?-49.0/48:-5.0/4)));
    REQUIRE(g.second==Approx(4*(c4?161.0/24:5.0)));
    const auto tail=e.density(1.25,'a',0);
    REQUIRE(tail.value==Approx(c4?125.0/8192:1.0/32));
    REQUIRE(tail.deriv==Approx(c4?-747.0/2048:-9.0/16));
    REQUIRE(e.density(0,'a',0).value==1);
    REQUIRE(e.density(3,'a',0).value==0);
  }
}

TEST_CASE("DM_Embed named derivatives include width Jacobians zero references and force chains",
          "[unified-dm][derivatives]") {
  for(const auto &family:{"Blip","Blip5","Gaussian","Knot5","WendlandC2","WendlandC4"})
    for(const auto &env:{"OFF","SinInv","Smoothstep2","Smoothstep4"})
    for(const auto &form:{"NONNEG","MONOTONE"}) {
      if(std::string(family)=="Knot5" && std::string(form)=="MONOTONE")continue;
      auto c=embed_config(family,family,form,"Cut_Poly",env,true);
      DC_Selector s(c); const double r=1.37,h=2e-6;
      for(const auto &binding:embed_bindings(c)) {
        CAPTURE(family,env,binding.path);
        const std::size_t index=binding.path.find(".EMBEDDING.")!=std::string::npos && binding.values.size()>1?1:0;
        const int pos=static_cast<int>(index+1);
        auto plus=c,minus=c;
        binding.write(plus,index,binding.values[index]+h);
        binding.write(minus,index,binding.values[index]-h);
        DC_Selector p(plus),m(minus);
        const double fd=(energy(*p.dmb,r)-energy(*m.dmb,r))/(2*h);
        rho_type rho,drho; s.dmb->init_rhoi(rho); s.dmb->init_rhoi(drho);
        s.dmb->calc_rho(73,73,r,r*r,Vec3d(r,0,0),rho);
        aed_type a(3),da(3); a.set_zero(); da.set_zero(); s.dmb->calc_aed(rho,a);
        REQUIRE(s.dmb->calc_drhoi_dtheta(binding.path,pos,73,73,r,r*r,Vec3d(r,0,0),drho));
        REQUIRE(s.dmb->calc_daed_dtheta(binding.path,pos,rho,drho,da));
        double value=0; for(std::size_t j=0;j<3;++j) value+=(j+1)*da(j);
        REQUIRE(value==Approx(fd).margin(3e-7));
        auto force=[&](DM_Base &d) {
          rho_type pr; d.init_rhoi(pr); d.calc_rho(73,73,r,r*r,Vec3d(r,0,0),pr);
          aed_type pa(3); pa.set_zero(); d.calc_aed(pr,pa);
          fd_type pf(3); pf.set_zero(); d.calc_dXijdri(73,73,r,r*r,Vec3d(r,0,0),pr,pf);
          double out=0; for(std::size_t j=0;j<3;++j) out+=(j+1)*pf(j,0); return out;
        };
        fd_type df(3); df.set_zero();
        REQUIRE(s.dmb->calc_dXijdri_dtheta(binding.path,pos,73,73,r,r*r,Vec3d(r,0,0),rho,drho,df));
        value=0; for(std::size_t j=0;j<3;++j) value+=(j+1)*df(j,0);
        REQUIRE(value==Approx((force(*p.dmb)-force(*m.dmb))/(2*h)).margin(3e-7));
      }
    }
}

TEST_CASE("DM_Embed rejects inactive fields and unsafe supports without numerical heuristics",
          "[unified-dm][validation]") {
  auto good=embed_config();
  for(const auto &replacement:{
      "ETA 2 2\nWIDTHS 0.5 0.5", "ETA 2 2\nETA 2 2", "ETA 0 2",
      "ETA nan 2", "ETA 2", "BASIS Blip5\nBASIS Blip", "UNKNOWN 1"}) {
    auto c=good; auto b=c.blocks();
    for(auto &line:b[0].lines) if(line=="ETA 2 2") {line=replacement;break;}
    // Inserted lines must remain separate structured records, as a parser
    // would capture them.
    std::ostringstream text;
    for(const auto &line:b[0].lines) text<<line<<'\n';
    std::istringstream input(text.str());
    auto bad=CoreUserConfigParser().parse(input);
    REQUIRE_THROWS(DC_Selector(bad));
  }
  auto g=embed_config("Gaussian","Blip","NONNEG","NONE");
  REQUIRE_THROWS(DC_Selector(g));
  auto b=embed_config("Blip5","Blip","NONNEG","NONE");
  auto blocks=b.blocks();
  for(auto &line:blocks[0].lines) if(line=="CENTRES 1 2") line="CENTRES 6 7";
  b.replace_blocks(blocks); REQUIRE_THROWS(DC_Selector(b));
  auto signed_env=embed_config("Blip","Blip","SIGNED","Cut_Poly","Smoothstep2");
  DC_Selector s(signed_env);
  rho_type rho; s.dmb->init_rhoi(rho); rho(0)=-0.1;
  aed_type a(3);a.set_zero(); REQUIRE_THROWS(s.dmb->calc_aed(rho,a));
}

TEST_CASE("DM_Embed widths round trip exactly at every NUMERIC precision",
          "[unified-dm][persistence]") {
  auto c=embed_config("Gaussian","Gaussian","NONNEG","Cut_Poly","OFF",true);
  auto binding=embed_binding(c,"ta.embedding.widths");
  const double w=std::nextafter(0.12345678901234567,1.0);
  binding.write(c,0,w); DC_Selector original(c);
  for(int precision:{3,12,17,18}) {
    std::ostringstream text; c.write_potential(text,precision);
    std::istringstream in(text.str()); auto copy=CoreUserConfigParser().parse(in);
    REQUIRE(embed_binding(copy,"ta.embedding.widths").values[0]==w);
    DC_Selector loaded(copy);
    REQUIRE(energy(*original.dmb,1.27)==energy(*loaded.dmb,1.27));
  }
}

TEST_CASE("DM_Embed blocks compose in declaration order and require unique references",
          "[unified-dm][composition]") {
  auto a=embed_config("Gaussian","Blip5");
  auto b=embed_config("WendlandC2","Gaussian");
  auto second=embed_specs(b).at(0); second.id="tb";
  a.append_block(embed_block(second));
  DC_Selector combined(a);
  REQUIRE(combined.dmb->size()==6);
  rho_type rho; combined.dmb->init_rhoi(rho);
  REQUIRE(rho.size()==8);
  combined.dmb->calc_rho(73,73,1.37,1.37*1.37,Vec3d(1.37,0,0),rho);
  aed_type all(6);all.set_zero();combined.dmb->calc_aed(rho,all);
  for(std::size_t i=0;i<2;++i) {
    auto one=a; auto spec=embed_specs(a)[i]; one.replace_blocks({embed_block(spec)});
    one.remove("TYPEMB"); DC_Selector single(one);
    rho_type r;single.dmb->init_rhoi(r);
    single.dmb->calc_rho(73,73,1.37,1.37*1.37,Vec3d(1.37,0,0),r);
    aed_type e(3);e.set_zero();single.dmb->calc_aed(r,e);
    for(std::size_t m=0;m<3;++m) REQUIRE(all(3*i+m)==e(m));
  }
  for(const auto &refs:{"@ta", "@ta @ta", "@ta @missing", "@tb @ta @tb"}) {
    auto bad=a;bad.remove("TYPEMB");bad.add("TYPEMB",refs);
    REQUIRE_THROWS(DC_Selector(bad));
  }
}

TEST_CASE("Compact basis metadata uses inverse distance units", "[unified-dm][metadata]") {
  for(const auto &label:{"D2_Blip","D2_Blip5","DM_Blip","DM_REAM"}) {
    std::unique_ptr<D_Base> d(factory<D_Base>(label));
    auto meta=d->meta();REQUIRE(meta.has_value());
    bool checked=false;
    for(const auto &p:meta->params)if(p.key=="SGRID2B" || p.key=="SGRIDMB") {
      CAPTURE(label,p.key);REQUIRE(p.units=="Å^-1");checked=true;
    }
    REQUIRE(checked);
  }
}

TEST_CASE("DM_Embed composes with legacy terms in explicit order and an invalid component is refused", "[unified-dm][composition]") {
  auto fresh=embed_config("Gaussian","Blip5");auto spec=embed_specs(fresh).at(0);
  auto legacy=fresh;legacy.replace_blocks({});legacy.set_component_cutoffs({});legacy.remove("TYPEMB");
  legacy.add("TYPEMB","DM_REAM 0 1 1 1 2 2 0 0 Ta Ta");legacy.add("RHOFORM","SIGNED");legacy.add("EMBF0","FREE");
  legacy.add("CGRIDMB",2);legacy.add("SGRIDMB",2);legacy.add("AMPGRIDMB",0.3);
  legacy.add("CEMBFUNC","0 1");legacy.add("SEMBFUNC","2 2");legacy.add("RCUTMB",5);legacy.add("RCTYPEMB","Cut_Poly");
  DC_Selector old(legacy),modern(fresh);
  auto values=[](DM_Base &d) {rho_type r;d.init_rhoi(r);d.calc_rho(73,73,1.37,1.37*1.37,Vec3d(1.37,0,0),r);aed_type v(d.size());v.set_zero();d.calc_aed(r,v);return v;};
  auto a=values(*modern.dmb),b=values(*old.dmb);
  for(bool reverse:{false,true}) {
    auto joined=legacy;joined.append_block(embed_block(spec));joined.remove("TYPEMB");
    joined.add("TYPEMB",reverse?"DM_REAM 0 1 1 1 2 2 0 0 Ta Ta @ta":"@ta DM_REAM 0 1 1 1 2 2 0 0 Ta Ta");
    DC_Selector combined(joined);auto all=values(*combined.dmb);REQUIRE(all.size()==5);
    for(std::size_t i=0;i<a.size();++i)REQUIRE(all(i+(reverse?2:0))==a(i));
    for(std::size_t i=0;i<b.size();++i)REQUIRE(all(i+(reverse?0:3))==b(i));
    REQUIRE_FALSE(combined.dmb->embf0_mode().has_value());
    auto bad=joined;auto broken=spec;broken.density.amplitudes[0]=-1;bad.replace_blocks({embed_block(broken)});
    REQUIRE_THROWS(DC_Selector(bad));  // the constructor refuses it
  }
}

TEST_CASE("DM_Embed component metadata counts only its owned embedding columns", "[unified-dm][metadata]") {
  auto parent=embed_config();auto second=embed_specs(parent).at(0);second.id="tb";
  parent.append_block(embed_block(second));DC_Selector all(parent);SeedView seed(parent);
  for(const auto &child:embed_contexts(parent)) {
    std::unique_ptr<DM_Base> descriptor(factory<DM_Base,const Context&>(child.get<std::string>("TYPEMB"),child));
    auto meta=descriptor->meta();REQUIRE(meta.has_value());REQUIRE(bool(meta->expected_columns));
    REQUIRE(meta->expected_columns(seed)==descriptor->size());
  }
}

TEST_CASE("DM_Embed analytical routing refuses unknown components and zero vector indices", "[unified-dm][derivatives]") {
  auto c=embed_config();DC_Selector selector(c);rho_type derivative;selector.dmb->init_rhoi(derivative);
  for(const auto &path:{"ta.other.centres","ta.other.eta"})
    REQUIRE_FALSE(selector.dmb->calc_drhoi_dtheta(path,1,73,73,1.3,1.69,Vec3d(1.3,0,0),derivative));
  REQUIRE_FALSE(selector.dmb->calc_drhoi_dtheta("ta.density.centres",0,73,73,1.3,1.69,Vec3d(1.3,0,0),derivative));
}

#include <tadah/core/stats/data_stats.h>
#include <optional>

/* Pins: the density cutoff of a DM_Embed component (DENSITY CUTOFF RADIUS,
 * the metadata key TA.DENSITY.CUTOFF.RADIUS) has the check of every cutoff
 * (cutoff_reach_check): for the component of embed_config() (RADIUS 6), on
 * Ta distances from 2.25 A it passes, on Ta distances from 6.5 A it fails,
 * the message naming TA.DENSITY.CUTOFF.RADIUS, 6 and 6.5 A, and on data
 * with atoms but no pair it fails, naming the key; a component without a
 * cutoff (TYPE NONE) has no such key.
 * Before the fix: the metadata of DM_Embed had no check, and tadah train
 * under AUDIT error trained a component whose RADIUS holds no neighbour
 * distance, its density 0 for every atom. */
TEST_CASE("DM_Embed's density cutoff fails when it holds no neighbour "
          "distance",
          "[unified-dm][metadata][cutoff]") {
  auto ta_stats = [](double r_min) {
    tadah::core::stats::DataStats d;
    d.n_atoms = 100;
    d.n_structures = 10;
    d.elements = {"Ta"};
    tadah::core::stats::PerPairStats pp;
    pp.elements = {"Ta", "Ta"};
    pp.n_pairs = 1000;
    pp.r_min = r_min;
    pp.r_max = 7.5;
    for (std::size_t i = 0; i < pp.r_percentiles.size(); ++i)
      pp.r_percentiles[i] = r_min + 0.1 * static_cast<double>(i);
    d.per_pair.push_back(pp);
    return d;
  };
  auto radius_check = [](const Context &parent) -> const ParamMeta * {
    static std::optional<DescriptorMeta> meta;
    meta.reset();
    for (const auto &child : embed_contexts(parent)) {
      std::unique_ptr<DM_Base> descriptor(factory<DM_Base, const Context &>(
          child.get<std::string>("TYPEMB"), child));
      meta = descriptor->meta();
    }
    REQUIRE(meta.has_value());
    for (const auto &p : meta->params)
      if (p.key == "TA.DENSITY.CUTOFF.RADIUS") return &p;
    return nullptr;
  };
  const Context parent = embed_config();
  const ParamMeta *radius = radius_check(parent);
  REQUIRE(radius != nullptr);
  REQUIRE(static_cast<bool>(radius->check));
  const SeedView seed(parent);
  CHECK(is_ok(radius->check(ta_stats(2.25), seed)));
  const HealthVerdict below = radius->check(ta_stats(6.5), seed);
  const std::string message = verdict_message(below);
  INFO(message);
  CHECK(is_fail(below));
  CHECK(message.find("TA.DENSITY.CUTOFF.RADIUS = 6 ") != std::string::npos);
  CHECK(message.find("(6.5 Å)") != std::string::npos);
  tadah::core::stats::DataStats none = ta_stats(2.25);
  none.per_pair.clear();
  const HealthVerdict no_pair = radius->check(none, seed);
  INFO(verdict_message(no_pair));
  CHECK(is_fail(no_pair));
  CHECK(verdict_message(no_pair).find("TA.DENSITY.CUTOFF.RADIUS") !=
        std::string::npos);
  CHECK(radius_check(embed_config("Blip5", "Gaussian", "NONNEG", "NONE")) ==
        nullptr);
}
