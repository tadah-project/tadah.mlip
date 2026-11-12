/* hpo_test::unique_temp_path (scratch_tmpdir.h), the path a test takes for a
 * file or directory it creates in the temporary directory:
 * <temporary directory>/<stem><process id>_<n><ext>, n a count of the paths
 * the process has taken. CTest runs every test case in a process of its own,
 * many at a time: two processes that run side by side differ in the process
 * id, two calls of one process in n, whatever a clock reads.
 * Before the fix: the fixtures made the name from a clock reading alone,
 * which two processes that read the same tick share; this file did not
 * compile (no unique_temp_path). */

#include "catch2/catch.hpp"
#include "scratch_tmpdir.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;

const std::string kStem = "tadah_unique_temp_path_";

/* The count n of @p path, which must be
 * <temporary directory>/<kStem><this process's id>_<n><ext>. */
unsigned long count_of(const fs::path& path, const std::string& ext)
{
    /* The whole path: TMPDIR may be written with a '/' at its end, which
     * the parent of the path does not have. */
    REQUIRE(path == fs::temp_directory_path() / path.filename());
    const std::string name = path.filename().string();
    const std::string head = kStem + std::to_string(::getpid()) + "_";
    REQUIRE(name.rfind(head, 0) == 0);
    REQUIRE(name.size() > head.size() + ext.size());
    REQUIRE(name.substr(name.size() - ext.size()) == ext);
    const std::string n =
        name.substr(head.size(), name.size() - head.size() - ext.size());
    REQUIRE(n.find_first_not_of("0123456789") == std::string::npos);
    return std::stoul(n);
}

}  // namespace

TEST_CASE("unique_temp_path: the name holds the process id and a count of "
          "the paths the process has taken", "[HPO][scratch]")
{
    const fs::path a = hpo_test::unique_temp_path(kStem, ".log");
    const fs::path b = hpo_test::unique_temp_path(kStem, ".log");
    const fs::path c = hpo_test::unique_temp_path(kStem);
    const unsigned long n = count_of(a, ".log");
    CHECK(count_of(b, ".log") == n + 1);
    CHECK(count_of(c, "") == n + 2);
    /* Nothing is created. */
    CHECK_FALSE(fs::exists(a));
    CHECK_FALSE(fs::exists(b));
    CHECK_FALSE(fs::exists(c));
}

TEST_CASE("unique_temp_path: what a process of the same id left under the "
          "name is removed", "[HPO][scratch]")
{
    const unsigned long n = count_of(hpo_test::unique_temp_path(kStem), "");
    /* The next path, as a process that was killed left it: a directory
     * with a file, then a file. */
    for (unsigned long k = n + 1; k <= n + 2; ++k) {
        const fs::path left = fs::temp_directory_path() /
            (kStem + std::to_string(::getpid()) + "_" + std::to_string(k));
        if (k == n + 1) {
            fs::create_directories(left / "run");
            std::ofstream(left / "run" / "journal.log") << "another run\n";
        } else {
            std::ofstream(left) << "another run\n";
        }
        REQUIRE(fs::exists(left));
        CHECK(hpo_test::unique_temp_path(kStem) == left);
        CHECK_FALSE(fs::exists(left));
    }
}

TEST_CASE("unique_temp_path: a stem or an extension that is not part of a "
          "file name is refused, and nothing is removed", "[HPO][scratch]")
{
    /* A directory of this case, and in it what the next path would be for
     * a stem that reaches into the directory. */
    const fs::path dir = hpo_test::unique_temp_path(kStem);
    const std::string inside = dir.filename().string() + "/kept_";
    const fs::path kept = dir /
        ("kept_" + std::to_string(::getpid()) + "_" +
         std::to_string(count_of(dir, "") + 1));
    fs::create_directories(kept);
    std::ofstream(kept / "journal.log") << "a run\n";

    CHECK_THROWS_AS(hpo_test::unique_temp_path(inside),
                    std::invalid_argument);
    CHECK_THROWS_AS(hpo_test::unique_temp_path("../" + kStem),
                    std::invalid_argument);
    CHECK_THROWS_AS(hpo_test::unique_temp_path(dir.string() + "_"),
                    std::invalid_argument);   // absolute
    CHECK_THROWS_AS(hpo_test::unique_temp_path(".."),
                    std::invalid_argument);
    CHECK_THROWS_AS(hpo_test::unique_temp_path(kStem, "/journal.log"),
                    std::invalid_argument);
    CHECK_THROWS_AS(hpo_test::unique_temp_path(kStem, "/../" + kStem),
                    std::invalid_argument);
    /* "..._1" + 23 and "..._" + 123 are one name. */
    CHECK_THROWS_AS(hpo_test::unique_temp_path(kStem + "1"),
                    std::invalid_argument);
    CHECK(fs::exists(kept / "journal.log"));

    /* A refused call takes no count. */
    const fs::path next = hpo_test::unique_temp_path(kStem);
    CHECK(count_of(next, "") == count_of(dir, "") + 1);
    fs::remove_all(dir);
}
