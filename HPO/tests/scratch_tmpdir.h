#pragma once
/* unique_temp_path: a path in the temporary directory that no other test
 * process uses, for a file or a directory a test creates.
 *
 * ScratchTmpdir: point TMPDIR at a fresh, empty directory for the life of
 * one test, so the test can count what the code under test leaves in the
 * temporary directory without seeing other processes' files.
 *
 * std::filesystem::temp_directory_path() reads TMPDIR on every call, and a
 * PC_ISOLATION worker (fork + execv) inherits the environment at spawn, so
 * both the test process and the workers it starts afterwards use the
 * scratch directory. The destructor restores TMPDIR (or unsets it if it was
 * unset) and removes the directory. Not thread-safe: construct it before
 * the code under test starts threads or workers, destroy it after. */

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

#include <unistd.h>

namespace hpo_test {

/* <temporary directory>/<stem><process id>_<n><ext>, n counting the paths
 * this process has taken: two processes that run side by side differ in the
 * process id, two calls of one process in n. CTest runs every test case in
 * a process of its own, many at a time, and a name made from a clock
 * reading alone is the same in two of them that read the same tick. Nothing
 * is created; what a process that had the same id left there is removed.
 * The process id tells apart the processes of one process-id namespace: two
 * containers that share a temporary directory can hold the same one.
 * The stem and the extension are parts of one file name: std::invalid_argument
 * for one that holds '/' or "..", which would name (and remove) a path
 * outside the temporary directory, and for a stem that ends in a digit,
 * which would run into the process id ("run_1" + 23 is "run_" + 123). */
inline std::filesystem::path unique_temp_path(const std::string& stem,
                                              const std::string& ext = "")
{
    for (const std::string* part : {&stem, &ext})
        if (part->find('/') != std::string::npos ||
            part->find("..") != std::string::npos)
            throw std::invalid_argument(
                "unique_temp_path: '" + *part + "' is not part of a file "
                "name (it holds '/' or \"..\")");
    if (!stem.empty() && stem.back() >= '0' && stem.back() <= '9')
        throw std::invalid_argument(
            "unique_temp_path: the stem '" + stem + "' ends in a digit, "
            "which the process id would continue");
    static std::atomic<unsigned long> n{0};
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        (stem + std::to_string(::getpid()) + "_" + std::to_string(n++) + ext);
    std::filesystem::remove_all(path);
    return path;
}

class ScratchTmpdir {
public:
    explicit ScratchTmpdir(const std::string& tag)
    {
        namespace fs = std::filesystem;
        if (const char* old = std::getenv("TMPDIR")) old_ = std::string(old);
        dir_ = unique_temp_path("tadah_test_" + tag + "_");
        fs::create_directories(dir_);
        if (::setenv("TMPDIR", dir_.c_str(), 1) != 0)
            throw std::runtime_error("ScratchTmpdir: setenv TMPDIR failed");
        if (fs::temp_directory_path() != dir_)
            throw std::runtime_error("ScratchTmpdir: TMPDIR is not honoured");
    }

    ~ScratchTmpdir()
    {
        if (old_) ::setenv("TMPDIR", old_->c_str(), 1);
        else      ::unsetenv("TMPDIR");
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }

    ScratchTmpdir(const ScratchTmpdir&) = delete;
    ScratchTmpdir& operator=(const ScratchTmpdir&) = delete;

    const std::filesystem::path& path() const { return dir_; }

    /* Number of entries of the directory whose name starts with prefix. */
    std::size_t count(const std::string& prefix) const
    {
        std::size_t k = 0;
        for (const auto& e : std::filesystem::directory_iterator(dir_))
            if (e.path().filename().string().rfind(prefix, 0) == 0) ++k;
        return k;
    }

private:
    std::filesystem::path dir_;
    std::optional<std::string> old_;
};

}  // namespace hpo_test
