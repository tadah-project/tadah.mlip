// Stand-in for a Catch2 test executable, used by check_discovery.cmake and
// check_module.cmake to give cmake-tools' testing/CatchAddTests.cmake
// listings that a real Catch binary cannot produce on demand (256 names,
// CRLF line ends, no names, a crash, a count that disagrees with the output,
// unselectable names).
//
// With --list-test-names-only it copies the file named by
// TADAH_FAKE_LISTER_OUTPUT to stdout byte for byte, then exits with
// TADAH_FAKE_LISTER_EXIT: an integer, "abort" (dies on SIGABRT), or, when
// unset, the number of lines -- the count Catch returns, which the OS
// truncates modulo 256 (256 names exit with 0). When TADAH_FAKE_LISTER_ARGS
// names a file, each listing appends its arguments to it, one per line,
// followed by the value of TADAH_FAKE_LISTER_VIA (or "-" when unset) and an
// empty line.
//
// With --list-reporters it lists one reporter, "fake", in Catch's format.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
  bool list = false;
  bool reporters = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--list-test-names-only") == 0) list = true;
    if (std::strcmp(argv[i], "--list-reporters") == 0) reporters = true;
  }
  if (reporters) {
    std::printf("Available reporters:\n  fake:  the fake lister's reporter\n\n");
    return 1;
  }
  if (!list) {
    std::fprintf(stderr, "fake_lister: only --list-test-names-only\n");
    return 100;
  }
  if (const char* args_path = std::getenv("TADAH_FAKE_LISTER_ARGS")) {
    std::ofstream args(args_path, std::ios::binary | std::ios::app);
    for (int i = 1; i < argc; ++i) args << argv[i] << '\n';
    const char* via = std::getenv("TADAH_FAKE_LISTER_VIA");
    args << (via != nullptr ? via : "-") << "\n\n";
  }
  const char* path = std::getenv("TADAH_FAKE_LISTER_OUTPUT");
  if (path == nullptr) {
    std::fprintf(stderr, "fake_lister: TADAH_FAKE_LISTER_OUTPUT is unset\n");
    return 101;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    std::fprintf(stderr, "fake_lister: cannot read '%s'\n", path);
    return 102;
  }
  const std::string out((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
  std::fwrite(out.data(), 1, out.size(), stdout);
  std::fflush(stdout);

  const char* exit_spec = std::getenv("TADAH_FAKE_LISTER_EXIT");
  if (exit_spec != nullptr && std::strcmp(exit_spec, "abort") == 0)
    std::abort();
  if (exit_spec != nullptr && *exit_spec != '\0')
    return std::atoi(exit_spec);
  return static_cast<int>(std::count(out.begin(), out.end(), '\n'));
}
