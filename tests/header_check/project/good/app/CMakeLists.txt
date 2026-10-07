add_executable(probe_app app.cpp)
target_include_directories(probe_app
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_compile_definitions(probe_app PRIVATE PROBE_APP_DEFINITION)
target_link_libraries(probe_app PRIVATE probe)
