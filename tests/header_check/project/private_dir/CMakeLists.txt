add_library(probe STATIC probe.cpp)
target_include_directories(probe
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private")
