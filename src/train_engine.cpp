#include <tadah/engines/train_engine.h>
#include <tadah/core/config.h>
#include <tadah/mlip/StructureDB.h>
#include <tadah/mlip/Trainer.h>
#include <tadah/mlip/memory/MLIPWorkspaceManager.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>

namespace tadah {
namespace engines {

int TrainEngine::train(const tadah::core::Config &config) {
  try {
    tadah::mlip::memory::MLIPWorkspaceManager workspaceManager;
    tadah::mlip::Trainer trainer(config, workspaceManager);
    tadah::mlip::StructureDB stdb(trainer.config);
    trainer.nnf.calc(stdb);
    trainer.model->train(stdb, trainer.dc);
    tadah::core::Config param_file = trainer.model->get_param_file();
    param_file.check_pot_file();
    std::ofstream outfile("pot.tadah");
    if (!outfile.is_open())
      throw std::runtime_error("Unable to open file pot.tadah");
    outfile << param_file << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "TrainEngine error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

} // namespace engines
} // namespace tadah

