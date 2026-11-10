#pragma once
#include <tadah/models/descriptors/dm/dm_base.h>
#include <tadah/models/descriptors/dm/embed_kernel.h>

namespace tadah {
namespace models {
class DM_Embed : public DM_Base {
  std::unique_ptr<EmbedEvaluator> evaluator_;
  struct Parameter {
    bool owned = false, density = false;
    char field = ' ';
    std::size_t index = 0;
  };
  Parameter parameter(const std::string &, int) const;

public:
  DM_Embed();
  explicit DM_Embed(const core::Context &);
  void init() override;
  std::string label() const override { return "DM_Embed"; }
  const EmbedEvaluator &evaluator() const { return *evaluator_; }
  void init_rhoi(core::rho_type &) override;
  void calc_rho(int, int, double, double, const core::Vec3d &, core::rho_type &,
                double = 1) const override;
  void calc_aed(core::rho_type &, core::aed_type &) const override;
  void calc_dXijdri(int, int, double, double, const core::Vec3d &,
                    core::rho_type &, core::fd_type &,
                    double = 1) const override;
  void calc_dXijdri_dXjidri(int, int, double, double, const core::Vec3d &,
                            core::rho_type &, core::rho_type &, core::fd_type &,
                            double = 1) const override;
  bool eam_form() const override { return true; }
  std::size_t component_basis_size(const std::string &) const override;
  bool component_basis_curve(const std::string &, std::size_t, double, bool,
                             double &, double &) const override;
  bool component_cutoff(const std::string &, double, double &,
                        double &) const override;
  void eam_density(int, int, double, double &, double &) const override;
  void eam_embed(const core::t_type &, double, double &,
                 double &) const override;
  std::optional<EmbF0Mode> embf0_mode() const override;
  std::optional<double> raw_embedding_f0(const core::t_type &) const override;
  std::optional<DescriptorMeta> meta() const override;
  /** WATOM (D_Base::depends_on: its density is weighted by it), and a
   *  component path of its ID that names a parameter of its block
   *  (<ID>.DENSITY.CENTRES, ...), whether it differentiates it or not.
   *  Not RCUTMB: its cutoff is the block's (<ID>.DENSITY.CUTOFF.RADIUS). */
  bool depends_on(const std::string &, int) const override;
  bool calc_drhoi_dtheta(const std::string &, int, int, int, double, double,
                         const core::Vec3d &, core::rho_type &,
                         double = 1) const override;
  bool calc_daed_dtheta(const std::string &, int, const core::rho_type &,
                        const core::rho_type &, core::aed_type &,
                        double = 1) const override;
  bool calc_dXijdri_dtheta(const std::string &, int, int, int, double, double,
                           const core::Vec3d &, const core::rho_type &,
                           const core::rho_type &, core::fd_type &,
                           double = 1) const override;
};
} // namespace models
} // namespace tadah
