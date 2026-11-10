#ifndef TADAH_CORE_MEMORY_IWORKSPACEMANAGER_H
#define TADAH_CORE_MEMORY_IWORKSPACEMANAGER_H

namespace tadah {
namespace core {
namespace memory {

/**
 * @interface IWorkspaceManager
 * @brief Interface for workspace managers in the core module.
 *
 * This interface can be extended by other modules to manage memory allocations
 * and workspaces for computational classes.
 */
class IWorkspaceManager {
public:
  /**
   * @brief Virtual destructor for interface.
   */
  virtual ~IWorkspaceManager();
};

} // namespace memory
} // namespace core
} // namespace tadah

#endif // TADAH_CORE_MEMORY_IWORKSPACEMANAGER_H
