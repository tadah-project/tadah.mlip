# >>> TADAH_GIT_BASE (the same block in every Tadah! repository's
#     CMakeLists.txt, compared by cmake-tools' tadah_add_git_base_check)
# Base address of the Tadah! git repositories that FetchContent clones
# (<base>/<repository>.git): -DTADAH_GIT_BASE=<address>, else the
# environment variable TADAH_GIT_BASE when it is set and not empty, else
# the public repositories. A trailing '/' is dropped.
if(NOT DEFINED TADAH_GIT_BASE)
  if(NOT "$ENV{TADAH_GIT_BASE}" STREQUAL "")
    set(_tadah_git_base "$ENV{TADAH_GIT_BASE}")
  else()
    set(_tadah_git_base "https://github.com/tadah-project")
  endif()
  set(TADAH_GIT_BASE "${_tadah_git_base}" CACHE STRING
      "Base address of the Tadah! git repositories (<base>/<repository>.git)")
  unset(_tadah_git_base)
endif()
string(REGEX REPLACE "/+$" "" TADAH_GIT_BASE "${TADAH_GIT_BASE}")
# <<< TADAH_GIT_BASE
