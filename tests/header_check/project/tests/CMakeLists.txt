# As in a module's tests/CMakeLists.txt. The object files of each check are
# listed for the driver, which tells from them which headers compiled.
if(CASE STREQUAL "good" OR CASE STREQUAL "new_header")
  tadah_add_header_check(probe HEADER_DIRS
    "${PROJECT_SOURCE_DIR}/good/include"
    "${PROJECT_SOURCE_DIR}/good/generated")
  tadah_add_header_check(probe_app)
  set(checks probe probe_app)
elseif(CASE STREQUAL "private_entry")
  tadah_add_header_check(probe HEADER_DIRS
    "${PROJECT_SOURCE_DIR}/private_dir/include"
    "${PROJECT_SOURCE_DIR}/private_dir/private")
  set(checks probe)
else()
  tadah_add_header_check(probe)
  set(checks probe)
endif()
foreach(c IN LISTS checks)
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/objects_${c}.txt"
    CONTENT "$<JOIN:$<TARGET_OBJECTS:${c}_header_check>,\n>\n")
endforeach()
