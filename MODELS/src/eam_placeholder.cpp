#include <tadah/models/descriptors/eam_placeholder.h>

#include <tadah/core/key_notice.h>

namespace tadah {
namespace models {

void report_eam_placeholder(const tadah::core::Context &c,
                            const std::string &type_key,
                            const std::string &label) {
  const auto param = c.stored_value(type_key, 1);
  if (!param || *param == "0") return;
  tadah::core::report_ignored_key_once(
      type_key + " " + label + " parameter",
      type_key + " " + label + " parameter '" + *param + "'",
      "it is a placeholder (write 0); " + label +
          " reads its setfl file from SETFL.");
}

}  // namespace models
}  // namespace tadah
