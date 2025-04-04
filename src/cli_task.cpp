#include <tadah.mlip/cli/cli_task.h>
#include <algorithm>
#include <string>

namespace tadah {
namespace mlip {
namespace cli {

std::string CLITask::getTaskName() const {
  return blueprintTaskName;
}

std::unordered_map<std::string, std::vector<std::string>> CLITask::getKeyValueData() const {
  return storage;
}

} // namespace cli
} // namespace mlip
} // namespace tadah
