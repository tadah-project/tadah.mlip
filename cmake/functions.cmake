# Printing messages with a $TADAH prefix
function(tadah_message)
  # Define known message modes recognized by CMake
  set(_valid_modes 
    "FATAL_ERROR" "SEND_ERROR" "WARNING" "AUTHOR_WARNING"
    "DEPRECATION" "NOTICE" "STATUS" "VERBOSE"
  )

  # Default to STATUS mode if first argument is not a recognized mode
  list(FIND _valid_modes "${ARGV0}" _modeIndex)
  if(_modeIndex EQUAL -1)
    set(_mode "STATUS")
    set(_messageText "${ARGV}")
  else()
    set(_mode "${ARGV0}")
    list(REMOVE_AT ARGV 0)
    set(_messageText "${ARGV}")
  endif()

  # Concatenate remaining arguments into a single line 
  # (to mimic message() behavior with multiple arguments).
  string(REPLACE ";" " " _messageText "${_messageText}")

  # Print the message with "Tadah!: " prefix
  message(${_mode} "${TADAH} ${_messageText}")
endfunction()

function(check_build_type)
  string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE)
  if (CMAKE_BUILD_TYPE STREQUAL "DEBUG" 
      OR
      CMAKE_BUILD_TYPE STREQUAL "RELEASE"
      OR
      CMAKE_BUILD_TYPE STREQUAL "PROFILE"
      OR
      CMAKE_BUILD_TYPE STREQUAL "DEVELOP"
    )
    tadah_message(STATUS "Build type:  ${CMAKE_BUILD_TYPE}.")
  else()
    tadah_message(FATAL_ERROR "
    Selected build type is not supported: ${CMAKE_BUILD_TYPE}.
    Supported build types: Release, Debug, Profile Develop"
  )
endif()

# Update the documentation string of CMAKE_BUILD_TYPE for GUIs
set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
  "Choose the type of build, options are: None Debug Release Profile."
  FORCE)
endfunction()

# Deprecated: prefer cmake-tools `tadah_read_version_from_blueprint(VAR)` and
# explicit `project(<name> VERSION ${VAR})` at the call site.
# Kept as a thin shim for backwards compatibility.
function(set_project_name)
  if(COMMAND tadah_read_version_from_blueprint)
    tadah_read_version_from_blueprint(TADAH_VERSION REQUIRED)
  else()
    tadah_message(FATAL_ERROR
      "set_project_name() requires cmake-tools' tadah_read_version_from_blueprint.")
  endif()
  project(${TADAH} VERSION "${TADAH_VERSION}")
  tadah_message(STATUS "VERSION: ${TADAH_VERSION}")
endfunction()

function(find_directory_case_insensitive target_name search_dir result_var)
  execute_process(
    COMMAND find "${search_dir}" -maxdepth 1 -type d -iname "${target_name}"
    OUTPUT_VARIABLE found_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(found_dir)
    set(${result_var} "${found_dir}" PARENT_SCOPE)
    tadah_message("Develop build: Found directory: ${found_dir}")
  else()
    set(${result_var} "" PARENT_SCOPE)
    tadah_message("Develop build: Directory not found.")
  endif()
endfunction()
