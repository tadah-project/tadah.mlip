#ifndef TADAH_CORE_CHECKPOINT_H
#define TADAH_CORE_CHECKPOINT_H

/*
 *  Checkpoint — minimal, human-readable, atomically-committed key/value store.
 *
 *  Subsystems that want to persist state across runs implement
 *  ICheckpointable (save/load/tag) and register themselves with
 *  CheckpointManager (see checkpoint_manager.h).
 *
 *  File format
 *  -----------
 *      # Tadah Checkpoint v1
 *      # written: 2026-05-05T14:23:45Z
 *      [META]
 *      schema_version = 1
 *
 *      [OPTIMIZER_STATE]
 *      n_evals = 120
 *      best_loss = 1.23e-2
 *      best_params = 1.0 2.5 3e-3
 *      ...
 *
 *  Sections are delimited by [TAG] lines. Keys and values are separated
 *  by ' = '. Values are stored as text; readers parse them on demand.
 *  Vectors are space-separated on one line.
 *
 *  Atomicity — Checkpoint::commit() writes to <path>.tmp, fsyncs the file
 *  and the parent directory, and renames into place. A crash mid-write
 *  leaves either the previous good file or the .tmp; never a half-written
 *  primary file.
 */

#include <exception>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tadah {
namespace core {

/* Thrown by long-running loops when SignalHandler::stop_requested() turns
 * true. The HPO harness catches this in hpo_run() to flush its result
 * files and close the journal (log.finish()) before exiting. */
struct ShutdownRequested : public std::exception {
    const char* what() const noexcept override {
        return "shutdown requested (SIGINT/SIGTERM/SIGUSR2)";
    }
};

class Checkpoint {
public:
    explicit Checkpoint(std::string path);

    /* ---- writers ----------------------------------------------------- */
    /* Switch to a section. Subsequent put*() calls write under [name]. */
    void section(std::string name);
    void put       (const std::string& key, const std::string& val);
    void put_int   (const std::string& key, long long v);
    void put_size  (const std::string& key, std::size_t v);
    void put_dbl   (const std::string& key, double v);
    void put_bool  (const std::string& key, bool v);
    void put_vec   (const std::string& key, const std::vector<double>& v);
    void put_vec_int(const std::string& key, const std::vector<long long>& v);

    /* Append-style writer: deeper sections only need one read pass for
     * the manager, so we keep a simple ordered representation. */

    /* Atomic commit — writes <path>.tmp, fsyncs, renames into <path>.
     * Throws std::runtime_error if I/O fails. */
    void commit();

    /* ---- readers (after load_from_disk) ------------------------------ */
    /* Returns true if the file at the configured path was parsed
     * successfully into the in-memory store. Returns false (without
     * throwing) if the file does not exist; throws std::runtime_error
     * on parse error. */
    bool load_from_disk();

    bool has(const std::string& section, const std::string& key) const;
    std::optional<std::string> get(const std::string& section, const std::string& key) const;
    std::optional<long long>   get_int (const std::string& section, const std::string& key) const;
    std::optional<std::size_t> get_size(const std::string& section, const std::string& key) const;
    std::optional<double>      get_dbl (const std::string& section, const std::string& key) const;
    std::optional<bool>        get_bool(const std::string& section, const std::string& key) const;
    std::optional<std::vector<double>>    get_vec    (const std::string& section, const std::string& key) const;
    std::optional<std::vector<long long>> get_vec_int(const std::string& section, const std::string& key) const;

    /* Path on disk. */
    const std::string& path() const { return path_; }

    /* Inspect existence of file on disk without parsing. */
    static bool file_exists(const std::string& path);

private:
    using KV = std::map<std::string, std::string>;
    std::string                  path_;
    std::string                  cur_section_;
    /* Section -> ordered key/value pairs (preserve insertion order via
     * a paired vector; std::map gives lexical ordering on the read
     * side which is fine for our use cases). */
    std::map<std::string, KV>    sections_;

    void write_kv_(std::ostream&, const KV&);
    static std::string fmt_dbl_(double v);
};

/* Subsystem interface. Implementations should be cheap to call: save() in
 * particular may run from a between-stage flush in a long-running loop.
 * No I/O outside of `cp`. */
class ICheckpointable {
public:
    virtual ~ICheckpointable() = default;
    virtual void save(Checkpoint& cp) const = 0;
    virtual void load(const Checkpoint& cp) = 0;
    virtual std::string tag() const = 0;   // section prefix used by save()
};

}   // namespace core
}   // namespace tadah

#endif
