#include <tadah/core/checkpoint.h>
#include <tadah/core/number_format.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace tadah {
namespace core {

namespace {

std::string now_iso_utc()
{
    using clock = std::chrono::system_clock;
    std::time_t t = clock::to_time_t(clock::now());
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string trim_inplace_copy(std::string s)
{
    auto issp = [](unsigned char c){ return std::isspace(c); };
    while (!s.empty() && issp(static_cast<unsigned char>(s.back())))  s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && issp(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

void fsync_dir_of(const std::string& path)
{
    /* Best-effort: durability depends on the filesystem honouring
     * directory fsync. Failure here is non-fatal — atomicity already
     * comes from rename(); fsync only protects against power loss.  */
    std::string dir = ".";
    auto pos = path.find_last_of('/');
    if (pos != std::string::npos) dir = path.substr(0, pos);
    if (dir.empty()) dir = ".";
    int dfd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (dfd >= 0) {
        ::fsync(dfd);
        ::close(dfd);
    }
}

}  // namespace

Checkpoint::Checkpoint(std::string path) : path_(std::move(path)) {}

void Checkpoint::section(std::string name)
{
    cur_section_ = std::move(name);
    /* Touch the section so empty sections are still emitted (rare, but
     * lets a participant signal "I exist but have no state" without
     * special-casing the reader). */
    sections_[cur_section_];
}

void Checkpoint::put(const std::string& key, const std::string& val)
{
    if (cur_section_.empty())
        throw std::runtime_error("Checkpoint::put before section()");
    sections_[cur_section_][key] = val;
}

void Checkpoint::put_int(const std::string& key, long long v)
{
    put(key, std::to_string(v));
}

void Checkpoint::put_size(const std::string& key, std::size_t v)
{
    put(key, std::to_string(v));
}

void Checkpoint::put_dbl(const std::string& key, double v)
{
    put(key, fmt_dbl_(v));
}

void Checkpoint::put_bool(const std::string& key, bool v)
{
    put(key, v ? std::string("true") : std::string("false"));
}

void Checkpoint::put_vec(const std::string& key, const std::vector<double>& v)
{
    std::ostringstream oss;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) oss << ' ';
        oss << fmt_dbl_(v[i]);
    }
    put(key, oss.str());
}

void Checkpoint::put_vec_int(const std::string& key, const std::vector<long long>& v)
{
    std::ostringstream oss;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) oss << ' ';
        oss << v[i];
    }
    put(key, oss.str());
}

std::string Checkpoint::fmt_dbl_(double v)
{
    /* 17 significant digits round-trips IEEE 754 double; get_dbl accepts
     * anything std::stod handles, plus subnormals (core::to_double). */
    std::ostringstream oss;
    oss << std::setprecision(17) << v;
    return oss.str();
}

void Checkpoint::write_kv_(std::ostream& os, const KV& kv)
{
    for (const auto& [k, v] : kv) os << k << " = " << v << '\n';
}

void Checkpoint::commit()
{
    const std::string tmp = path_ + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            throw std::runtime_error("Checkpoint::commit: cannot open " + tmp +
                                     " for writing: " + std::strerror(errno));
        }
        f << "# Tadah Checkpoint v1\n";
        f << "# written: " << now_iso_utc() << "\n\n";
        for (const auto& [name, kv] : sections_) {
            f << '[' << name << "]\n";
            write_kv_(f, kv);
            f << '\n';
        }
        f.flush();
        if (!f) {
            throw std::runtime_error("Checkpoint::commit: write error on " + tmp);
        }
    }
    /* fsync file then rename. */
    int fd = ::open(tmp.c_str(), O_WRONLY);
    if (fd >= 0) { ::fsync(fd); ::close(fd); }
    if (::rename(tmp.c_str(), path_.c_str()) != 0) {
        throw std::runtime_error("Checkpoint::commit: rename(" + tmp + " -> " +
                                 path_ + ") failed: " + std::strerror(errno));
    }
    fsync_dir_of(path_);
}

bool Checkpoint::file_exists(const std::string& path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool Checkpoint::load_from_disk()
{
    sections_.clear();
    if (!file_exists(path_)) return false;
    std::ifstream f(path_);
    if (!f) {
        throw std::runtime_error("Checkpoint::load_from_disk: cannot open " +
                                 path_ + " for reading");
    }
    std::string line;
    std::string sec;
    std::size_t lineno = 0;
    while (std::getline(f, line)) {
        ++lineno;
        std::string s = trim_inplace_copy(line);
        if (s.empty() || s[0] == '#') continue;
        if (s.front() == '[' && s.back() == ']') {
            sec = s.substr(1, s.size() - 2);
            sections_[sec];   // ensure exists
            continue;
        }
        if (sec.empty()) {
            throw std::runtime_error("Checkpoint::load_from_disk: key/value "
                                     "before any [section] at line " +
                                     std::to_string(lineno));
        }
        auto eq = s.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error("Checkpoint::load_from_disk: malformed "
                                     "line " + std::to_string(lineno) +
                                     " (no '='): " + s);
        }
        std::string k = trim_inplace_copy(s.substr(0, eq));
        std::string v = trim_inplace_copy(s.substr(eq + 1));
        sections_[sec][k] = v;
    }
    return true;
}

bool Checkpoint::has(const std::string& section, const std::string& key) const
{
    auto it = sections_.find(section);
    if (it == sections_.end()) return false;
    return it->second.count(key) > 0;
}

std::optional<std::string>
Checkpoint::get(const std::string& section, const std::string& key) const
{
    auto it = sections_.find(section);
    if (it == sections_.end()) return std::nullopt;
    auto kv = it->second.find(key);
    if (kv == it->second.end()) return std::nullopt;
    return kv->second;
}

std::optional<long long>
Checkpoint::get_int(const std::string& section, const std::string& key) const
{
    auto s = get(section, key);
    if (!s) return std::nullopt;
    try { return std::stoll(*s); } catch (...) { return std::nullopt; }
}

std::optional<std::size_t>
Checkpoint::get_size(const std::string& section, const std::string& key) const
{
    auto s = get(section, key);
    if (!s) return std::nullopt;
    try {
        long long v = std::stoll(*s);
        if (v < 0) return std::nullopt;
        return static_cast<std::size_t>(v);
    } catch (...) { return std::nullopt; }
}

std::optional<double>
Checkpoint::get_dbl(const std::string& section, const std::string& key) const
{
    auto s = get(section, key);
    if (!s) return std::nullopt;
    // to_double: std::stod that also accepts an underflowing (subnormal)
    // value, which fmt_dbl_ writes for one.
    try { return tadah::core::to_double(*s); } catch (...) { return std::nullopt; }
}

std::optional<bool>
Checkpoint::get_bool(const std::string& section, const std::string& key) const
{
    auto s = get(section, key);
    if (!s) return std::nullopt;
    if (*s == "true" || *s == "1")  return true;
    if (*s == "false" || *s == "0") return false;
    return std::nullopt;
}

std::optional<std::vector<double>>
Checkpoint::get_vec(const std::string& section, const std::string& key) const
{
    auto s = get(section, key);
    if (!s) return std::nullopt;
    std::istringstream iss(*s);
    std::vector<double> v;
    double x;
    while (iss >> x) v.push_back(x);
    if (!iss.eof() && iss.fail()) return std::nullopt;
    return v;
}

std::optional<std::vector<long long>>
Checkpoint::get_vec_int(const std::string& section, const std::string& key) const
{
    auto s = get(section, key);
    if (!s) return std::nullopt;
    std::istringstream iss(*s);
    std::vector<long long> v;
    long long x;
    while (iss >> x) v.push_back(x);
    if (!iss.eof() && iss.fail()) return std::nullopt;
    return v;
}

}   // namespace core
}   // namespace tadah
