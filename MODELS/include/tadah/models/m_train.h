#ifndef M_TRAIN_H
#define M_TRAIN_H

#include <tadah/core/core_types.h>
#include <tadah/core/utils.h>
#include <tadah/models/memory/imodels_workspace_manager.h>
#include <tadah/models/memory/models_workspace_manager.h>

namespace tadah {
namespace models {

/**
 * @class M_Train
 * @brief Abstract base class for training models.
 *
 * Defines the interface for training models with a design matrix and target values.
 */
class M_Train {
protected:
  bool own_workspace_manager_ = false;
  tadah::models::memory::IModelsWorkspaceManager* workspaceManager_ = nullptr;

public:
  M_Train():
    own_workspace_manager_(true),
    workspaceManager_(new tadah::models::memory::ModelsWorkspaceManager())
  {}
  M_Train(tadah::models::memory::IModelsWorkspaceManager &workspaceManager):
    own_workspace_manager_(false),
    workspaceManager_(&workspaceManager)
  {}
  /**
     * @brief Virtual destructor for polymorphic deletion.
     */
  virtual ~M_Train() {
    if (workspaceManager_ != nullptr && own_workspace_manager_) {
      delete workspaceManager_;
      workspaceManager_ = nullptr;
      own_workspace_manager_ = false;
    }
  }

  /**
     * @brief Pure virtual function to train the model.
     *
     * Must be implemented by derived classes.
     *
     * @param Phi Design matrix containing input features.
     * @param T Target vector for training.
     */
  virtual void train(tadah::core::phi_type &Phi, tadah::core::t_type &T) = 0;

  /**
 * Computes the residual target for fixed-weight training and rearranges Phi.
 *
 * The residual vector T_r = T - sum_k Phi[:, indices[k]] * w_f[k] is computed
 * on the ORIGINAL column layout, so that w_f[k] pairs with column indices[k]
 * regardless of the declaration order of the indices. Phi is then rearranged
 * such that the indices.size() rightmost columns are the fixed ones (in
 * ascending original-column order — NOT in indices order; do not pair the
 * tail columns with w_f positionally).
 * The function returns move_map with move_map[new_pos] = original column
 * index of the column now at new_pos.
 *
 * @param Phi         The full matrix; on exit its columns are rearranged with
 *                    the fixed columns moved to the right.
 * @param T           The full target vector.
 * @param indices     0-based column indices that are fixed; indices[k] pairs
 *                    with w_f[k].
 * @param w_f         Fixed weight values, one per entry of indices.
 * @param T_r         Output residual target for the reduced regression.
 */
  template <typename M>
  std::vector<size_t> prep_train_with_residuals(
    tadah::core::Matrix_Base<M> &Phi, tadah::core::t_type &T, std::vector<size_t> &indices, const tadah::core::t_type w_f, tadah::core::t_type &T_r) {

    if (w_f.size() != indices.size()) {
      throw std::runtime_error("Size of w_f must be equal to size of indices.");
    }

    if (*std::max_element(indices.begin(), indices.end()) >= Phi.cols()) {
      throw std::runtime_error("Largest index is greater than the number of columns in Phi.");
    }

    T_r = T;
    for (size_t k = 0; k < indices.size(); ++k) {
      for (size_t i = 0; i < Phi.rows(); ++i) {
        T_r[i] -= Phi(i, indices[k]) * w_f[k];
      }
    }

    return move_columns_in_place(Phi, indices);
  }

  /**
 * The intercept columns of the regression after FIXINDEX: the positions j
 * < n_free of the free columns (move_map[j], their original column, as
 * prep_train_with_residuals returns it) that are among the function's
 * intercept columns `intercepts` (original indices). A pinned intercept
 * leaves the regression with its column; every other free column keeps
 * the ridge penalty, wherever it moved.
 */
  static std::vector<size_t> free_intercept_columns(
      const std::vector<size_t> &intercepts,
      const std::vector<size_t> &move_map, size_t n_free) {
    std::vector<size_t> out;
    for (size_t j = 0; j < n_free; ++j)
      for (size_t c : intercepts)
        if (move_map[j] == c) {
          out.push_back(j);
          break;
        }
    return out;
  }

  void setWorkspaceManager(tadah::models::memory::IModelsWorkspaceManager  *workspaceManager) {
    if (workspaceManager_ != nullptr && own_workspace_manager_) {
      delete workspaceManager_;
    }
    workspaceManager_ = workspaceManager;
  }


};

} // namespace models
} // namespace tadah
#endif // M_TRAIN_H
