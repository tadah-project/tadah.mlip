# Catch2 and its test discovery come from cmake-tools (tadah_setup_tests()).
tadah_setup_tests()

file(GLOB tests_src CONFIGURE_DEPENDS "*.cpp")
#file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests_data DESTINATION .)

add_executable(test_md ${tests_src})
target_link_libraries(test_md PRIVATE 
    Catch2 tadah.md tadah.core tadah.models) 

include(Catch)
catch_discover_tests(test_md)

# tadah.md_headers (ctest -L headers): every header of include/
# compiles on its own and twice in a row (cmake-tools'
# tadah_add_header_check). Before the fix: passes as well (a pin: no MD
# header failed when the check was added).
tadah_add_header_check(tadah.md)

# tadah.md_text (ctest -L text): no line of the repository's text files
# carries a person's initials, a reference to co-workers, a decision
# attributed to the code's owner, the name of an AI tool, a note kept
# outside the repository or filler wording (cmake-tools'
# tadah_add_text_check). Before the fix: fails, 1 line: .gitignore's entry for an
# AI tool's state directory.
tadah_add_text_check(tadah.md)

# tadah.md_git_base (label cmake): the top-level CMakeLists.txt carries
# cmake-tools' TADAH_GIT_BASE block and fetches every Tadah! repository as
# ${TADAH_GIT_BASE}/<repository>.git (testing/tadah_git_base_check.cmake).
# Before the fix: fails (no block; the repositories named by the address of
# the development host).
tadah_add_git_base_check(tadah.md)
