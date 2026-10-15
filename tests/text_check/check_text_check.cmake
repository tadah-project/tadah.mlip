# Check of cmake-tools' tadah_add_text_check() (cmake/TadahTesting.cmake)
# and of the script its entries run (testing/tadah_text_check.cmake), on
# throw-away trees written under WORK_DIR. The phrases the rules refuse are
# assembled here from pieces, so this file matches none of them.
#
#   cmake -DTADAH_CMAKE_DIR=<cmake-tools> -DWORK_DIR=<scratch>
#         -DGENERATOR=<CMake generator> -DCTEST=<ctest> [-DGIT=<git>]
#         -P check_text_check.cmake
#
# The script, run on a tree:
#   clean       harmless text in every kind of file it reads (MKL, twelve,
#               airplanes/, a ';' and brackets): it passes and counts the
#               files it read
#   one rule    for each rule a tree with one line that breaks it, in a
#               different kind of file each time (a .cpp, a .md, a
#               CHANGELOG.md, a .gitignore, a .h, a README.md, and a
#               configuration file without an extension): it fails, naming
#               that file and that rule, and counts 1 line
#   skipped     the same lines under .git, external, third_party, _deps,
#               catch2, build_x and cmake-build-debug, at any depth, in a
#               data file (.cif) and in a file without an extension larger
#               than 1 MiB: it passes and reads none of them
#   split       two lines that break a rule, one with a ';' inside brackets:
#               it fails and counts 2 lines, each reported on its own
#   git         with GIT, a root that is the top of a git work tree: a line
#               that breaks a rule in a file git ignores (out/run.txt) is
#               not read, one in a file git would track (new.md, not yet
#               added) is; a root inside a work tree (its sub/) reads every
#               file, the ignored ones too. Not run without GIT (it says so)
#   refusals    no TADAH_TEXT_ROOT, and a root that is not a directory
# The function, in a project configured with GENERATOR:
#   entry       tadah_add_text_check(probe) and (probe2 ROOT <dir>) add
#               probe_text and probe2_text, labelled text; `ctest -L text`
#               passes on a clean tree and fails, naming the file, once a
#               line that breaks a rule is added to the default root (the
#               project's source directory)
#   refusals    an unexpected argument, ROOT without a value and a ROOT that
#               is not a directory stop the configure step with their message
cmake_minimum_required(VERSION 3.16)

foreach(v TADAH_CMAKE_DIR WORK_DIR GENERATOR CTEST)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_text_check.cmake: -D${v}=... is required")
  endif()
endforeach()
set(script "${TADAH_CMAKE_DIR}/testing/tadah_text_check.cmake")
if(NOT EXISTS "${script}")
  message(FATAL_ERROR "check_text_check.cmake: no ${script}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(nfail 0)
macro(fail what)
  message(NOTICE "FAIL: ${what}")
  math(EXPR nfail "${nfail} + 1")
endmacro()

# One line that breaks each rule, from pieces.
string(CONCAT bad_initials "// checked (" "M" "K" ", the author)")
string(CONCAT bad_coworker "ask a " "col" "league")
string(CONCAT bad_decision "- Kept (" "own" "er decision)")
string(CONCAT bad_tool "." "cl" "aude/")
string(CONCAT bad_note "// see " "la" "nes/B/tmp/run.txt")
string(CONCAT bad_phrase "We " "del" "ve into it.")
set(rule_names "a person's initials" "a co-worker reference" "a decision note"
               "an AI tool" "a workspace note" "a stock phrase")
set(rule_lines "${bad_initials}" "${bad_coworker}" "${bad_decision}"
               "${bad_tool}" "${bad_note}" "${bad_phrase}")
set(rule_files "src/x.cpp" "docs/y.md" "CHANGELOG.md" ".gitignore"
               "tests/z.h" "README.md")

# run_script(<root> <result var> <output var>)
function(run_script root rvar ovar)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DTADAH_TEXT_ROOT=${root}"
                          -P "${script}"
                  RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
  set(${rvar} "${r}" PARENT_SCOPE)
  set(${ovar} "${o}${e}" PARENT_SCOPE)
endfunction()

function(write_clean root)
  file(WRITE "${root}/src/a.cpp" "// MKL and twelve airplanes/ a [b; c] d\nint a;\n")
  file(WRITE "${root}/docs/b.rst" "Text — with a dash.\n")
  file(WRITE "${root}/CMakeLists.txt" "project(p LANGUAGES NONE)\n")
  file(WRITE "${root}/LICENSE" "Licence text.\n")
  file(WRITE "${root}/.gitignore" "build/\n")
endfunction()

# --- the script: clean ---------------------------------------------------
set(root "${WORK_DIR}/clean")
write_clean("${root}")
run_script("${root}" r out)
if(NOT r EQUAL 0)
  fail("clean: the check failed:\n${out}")
elseif(NOT out MATCHES "0 lines in 5 text files of ")
  fail("clean: expected '0 lines in 5 text files of', got:\n${out}")
endif()

# --- the script: one rule ------------------------------------------------
foreach(i RANGE 5)
  list(GET rule_names ${i} name)
  list(GET rule_lines ${i} line)
  list(GET rule_files ${i} rel)
  set(root "${WORK_DIR}/rule${i}")
  write_clean("${root}")
  file(APPEND "${root}/${rel}" "${line}\n")
  run_script("${root}" r out)
  string(REGEX REPLACE "([][+.*()^$?|\\\\])" "\\\\\\1" rel_re "${rel}")
  if(r EQUAL 0)
    fail("rule '${name}': the check passed on ${rel}:\n${out}")
  elseif(NOT out MATCHES "(^|\n)${rel_re}: ${name}: ")
    fail("rule '${name}': no line '${rel}: ${name}: ' in:\n${out}")
  elseif(NOT out MATCHES "1 lines in ")
    fail("rule '${name}': expected '1 lines in', got:\n${out}")
  endif()
endforeach()
# A configuration file without an extension is read.
set(root "${WORK_DIR}/noext")
write_clean("${root}")
file(WRITE "${root}/examples/config" "# ${bad_coworker}\nDBFILE db.train\n")
run_script("${root}" r out)
if(r EQUAL 0 OR NOT out MATCHES "(^|\n)examples/config: a co-worker reference: ")
  fail("noext: examples/config not reported:\n${out}")
endif()

# --- the script: skipped -------------------------------------------------
set(root "${WORK_DIR}/skipped")
write_clean("${root}")
foreach(dir .git external third_party _deps catch2 build_x cmake-build-debug
            src/external src/deep/build)
  file(WRITE "${root}/${dir}/f.cpp" "${bad_initials}\n${bad_phrase}\n")
endforeach()
file(WRITE "${root}/data/x.cif" "${bad_initials}\n")
string(REPEAT "x" 1048577 big)
file(WRITE "${root}/data/OUTCAR" "${bad_initials}\n${big}\n")
run_script("${root}" r out)
if(NOT r EQUAL 0)
  fail("skipped: the check failed:\n${out}")
elseif(NOT out MATCHES "0 lines in 5 text files of ")
  fail("skipped: expected '0 lines in 5 text files of', got:\n${out}")
endif()

# --- the script: split ---------------------------------------------------
set(root "${WORK_DIR}/split")
write_clean("${root}")
string(CONCAT split_line "a [b; " "${bad_initials}" " c] d")
file(APPEND "${root}/src/a.cpp" "${split_line}\nint b;\n${bad_initials}\n")
run_script("${root}" r out)
if(r EQUAL 0)
  fail("split: the check passed:\n${out}")
elseif(NOT out MATCHES "2 lines in ")
  fail("split: expected '2 lines in', got:\n${out}")
endif()

# --- the script: git -----------------------------------------------------
if(DEFINED GIT AND NOT "${GIT}" STREQUAL "")
  set(repo "${WORK_DIR}/git")
  write_clean("${repo}")
  file(WRITE "${repo}/.gitignore" "out/\n")
  file(WRITE "${repo}/out/run.txt" "${bad_note}\n")
  file(WRITE "${repo}/sub/out/run.txt" "${bad_note}\n")
  file(WRITE "${repo}/sub/notes.md" "Plain notes.\n")
  execute_process(COMMAND "${GIT}" init -q "${repo}" RESULT_VARIABLE r
                  OUTPUT_VARIABLE o ERROR_VARIABLE e)
  if(NOT r EQUAL 0)
    fail("git: git init failed:\n${o}${e}")
  else()
    execute_process(COMMAND "${GIT}" -C "${repo}" add src docs CMakeLists.txt
                            LICENSE .gitignore RESULT_VARIABLE r
                    OUTPUT_VARIABLE o ERROR_VARIABLE e)
    if(NOT r EQUAL 0)
      fail("git: git add failed:\n${o}${e}")
    endif()
    # Ignored files only: nothing is read that breaks a rule.
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DTADAH_TEXT_ROOT=${repo}"
                            "-DGIT=${GIT}" -P "${script}"
                    RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
    set(out "${o}${e}")
    if(NOT r EQUAL 0)
      fail("git: the check read an ignored file:\n${out}")
    elseif(NOT out MATCHES "\\(the files git tracks or would track\\)")
      fail("git: the check did not read git's list of files:\n${out}")
    endif()
    # A file git would track, not yet added, is read.
    file(WRITE "${repo}/new.md" "${bad_phrase}\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DTADAH_TEXT_ROOT=${repo}"
                            "-DGIT=${GIT}" -P "${script}"
                    RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
    set(out "${o}${e}")
    if(r EQUAL 0 OR NOT out MATCHES "(^|\n)new\\.md: a stock phrase: " OR
       out MATCHES "out/run\\.txt")
      fail("git: expected new.md, and not out/run.txt, reported:\n${out}")
    endif()
    # A root inside the work tree is walked: its ignored file is read.
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DTADAH_TEXT_ROOT=${repo}/sub"
                            "-DGIT=${GIT}" -P "${script}"
                    RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
    set(out "${o}${e}")
    if(r EQUAL 0 OR NOT out MATCHES "(^|\n)out/run\\.txt: a workspace note: ")
      fail("git: a root inside the work tree did not read out/run.txt:\n${out}")
    endif()
  endif()
else()
  message(NOTICE "git cases not run: no -DGIT=<git>")
endif()

# --- the script: refusals ------------------------------------------------
execute_process(COMMAND "${CMAKE_COMMAND}" -P "${script}"
                RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
if(r EQUAL 0 OR NOT "${o}${e}" MATCHES "-DTADAH_TEXT_ROOT=<dir> is required")
  fail("refusal without TADAH_TEXT_ROOT:\n${o}${e}")
endif()
run_script("${WORK_DIR}/no_such_dir" r out)
string(REGEX REPLACE "\n *" " " out "${out}")
if(r EQUAL 0 OR NOT out MATCHES "no_such_dir is not a directory")
  fail("refusal of a missing root:\n${out}")
endif()

# --- the function: entry -------------------------------------------------
set(proj "${WORK_DIR}/project")
write_clean("${proj}")
file(MAKE_DIRECTORY "${WORK_DIR}/other_root")
file(WRITE "${WORK_DIR}/other_root/notes.md" "Plain notes.\n")
# The paths come in as cache entries, so that no path of this machine is
# written into the files the entry reads.
file(WRITE "${proj}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.15)\n"
  "project(probe LANGUAGES NONE)\n"
  "enable_testing()\n"
  "include(\"\${TOOLS}/cmake/TadahTesting.cmake\")\n"
  "tadah_add_text_check(probe)\n"
  "tadah_add_text_check(probe2 ROOT \"\${OTHER_ROOT}\")\n")
set(build "${WORK_DIR}/project_build")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${proj}" -B "${build}"
                        -G "${GENERATOR}" "-DTOOLS=${TADAH_CMAKE_DIR}"
                        "-DOTHER_ROOT=${WORK_DIR}/other_root"
                RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
if(NOT r EQUAL 0)
  fail("entry: the project did not configure:\n${o}${e}")
else()
  execute_process(COMMAND "${CTEST}" -N -L text WORKING_DIRECTORY "${build}"
                  RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
  if(NOT o MATCHES "Test +#[0-9]+: probe_text\n" OR
     NOT o MATCHES "Test +#[0-9]+: probe2_text\n" OR
     NOT o MATCHES "Total Tests: 2\n")
    fail("entry: ctest -N -L text does not list probe_text and probe2_text:\n${o}${e}")
  endif()
  execute_process(COMMAND "${CTEST}" -L text --output-on-failure
                  WORKING_DIRECTORY "${build}"
                  RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
  if(NOT r EQUAL 0)
    fail("entry: ctest -L text failed on a clean tree:\n${o}${e}")
  endif()
  file(APPEND "${proj}/docs/b.rst" "${bad_phrase}\n")
  execute_process(COMMAND "${CTEST}" -L text --output-on-failure
                  WORKING_DIRECTORY "${build}"
                  RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
  if(r EQUAL 0 OR NOT o MATCHES "docs/b\\.rst: a stock phrase: ")
    fail("entry: ctest -L text did not fail on docs/b.rst:\n${o}${e}")
  endif()
endif()

# --- the function: refusals ----------------------------------------------
set(calls "tadah_add_text_check(probe EXTRA)"
          "tadah_add_text_check(probe ROOT)"
          "tadah_add_text_check(probe ROOT \"${WORK_DIR}/no_such_dir\")")
set(messages "tadah_add_text_check\\(probe\\): unexpected arguments: EXTRA"
             "tadah_add_text_check\\(probe\\): ROOT names no directory"
             "tadah_add_text_check\\(probe\\): ROOT [^\n]*no_such_dir is not a directory")
foreach(i RANGE 2)
  list(GET calls ${i} call)
  list(GET messages ${i} msg)
  set(rproj "${WORK_DIR}/refusal${i}")
  file(WRITE "${rproj}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.15)\n"
    "project(probe LANGUAGES NONE)\n"
    "enable_testing()\n"
    "include(\"\${TOOLS}/cmake/TadahTesting.cmake\")\n"
    "${call}\n")
  execute_process(COMMAND "${CMAKE_COMMAND}" -S "${rproj}"
                          -B "${rproj}/build" -G "${GENERATOR}"
                          "-DTOOLS=${TADAH_CMAKE_DIR}"
                  RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
  string(REGEX REPLACE "\n *" " " joined "${o}${e}")
  if(r EQUAL 0 OR NOT joined MATCHES "${msg}")
    fail("refusal '${call}': expected a configure error matching '${msg}', got:\n${o}${e}")
  endif()
endforeach()

if(nfail GREATER 0)
  message(FATAL_ERROR "${nfail} checks of tadah_add_text_check failed")
endif()
message(NOTICE "every check of tadah_add_text_check passed")
