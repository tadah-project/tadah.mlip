#include <tadah/core/context.h>
#include <tadah/core/logger.h>
#include <tadah/core/number_format.h>
#include <tadah/core/retired_keys.h>

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace tadah {
namespace core {

namespace {
// A whitespace-free, lossless record keeps existing Context transports
// (including per-key PC deltas) complete without interpreting block fields.
std::string hex_encode(const std::string &s) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out; out.reserve(2 * s.size());
  for (unsigned char c : s) { out += digits[c >> 4]; out += digits[c & 15]; }
  return out;
}
std::string hex_decode(const std::string &s) {
  if (s.size() % 2) throw std::runtime_error("Malformed structured block record");
  auto digit = [](char c) -> unsigned {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    throw std::runtime_error("Malformed structured block record");
  };
  std::string out; out.reserve(s.size() / 2);
  for (std::size_t i = 0; i < s.size(); i += 2)
    out += static_cast<char>(digit(s[i]) * 16 + digit(s[i + 1]));
  return out;
}
std::string encode_block(const ConfigBlock &b) {
  std::string body;
  for (const auto &line : b.lines) body += line + "\n";
  return hex_encode(b.kind) + ":" + hex_encode(b.source) + ":" +
         std::to_string(b.first_line) + ":" + hex_encode(body);
}
ConfigBlock decode_block(const std::string &s) {
  const auto a = s.find(':'), b = s.find(':', a + 1), c = s.find(':', b + 1);
  if (a == std::string::npos || b == std::string::npos || c == std::string::npos)
    throw std::runtime_error("Malformed structured block record");
  ConfigBlock out;
  out.kind = hex_decode(s.substr(0, a));
  out.source = hex_decode(s.substr(a + 1, b - a - 1));
  out.first_line = std::stoull(s.substr(b + 1, c - b - 1));
  std::istringstream body(hex_decode(s.substr(c + 1)));
  std::string line;
  while (std::getline(body, line)) out.lines.push_back(line);
  return out;
}
/* A retired line as one whitespace-free record: key, source, line, values
 * (one per line) and text, the strings hex-encoded. */
std::string encode_retired(const RetiredKeyLine &r) {
  std::string values;
  for (const auto &v : r.values) values += v + "\n";
  return hex_encode(r.key) + ":" + hex_encode(r.source) + ":" +
         std::to_string(r.line) + ":" + hex_encode(values) + ":" +
         hex_encode(r.text);
}
RetiredKeyLine decode_retired(const std::string &s) {
  std::vector<std::size_t> colon;
  for (std::size_t i = 0; i < s.size(); ++i)
    if (s[i] == ':') colon.push_back(i);
  if (colon.size() != 4)
    throw std::runtime_error("Malformed retired-key record");
  auto part = [&](std::size_t k) {
    const std::size_t b = k == 0 ? 0 : colon[k - 1] + 1;
    const std::size_t e = k < colon.size() ? colon[k] : s.size();
    return s.substr(b, e - b);
  };
  RetiredKeyLine r;
  r.key = hex_decode(part(0));
  r.source = hex_decode(part(1));
  r.line = std::stoull(part(2));
  std::istringstream values(hex_decode(part(3)));
  for (std::string v; std::getline(values, v);) r.values.push_back(v);
  r.text = hex_decode(part(4));
  return r;
}
} // namespace

void Context::record_retired_key(const RetiredKeyLine &r) {
  const BlueprintKey *bk = findBlueprintKey(to_upper(r.key), "ALLOWED_KEYS");
  if (bk == nullptr || !bk->isRetired())
    throw std::invalid_argument("Context::record_retired_key: " + r.key +
                                " is not a retired key");
  RetiredKeyLine up = r;
  up.key = to_upper(r.key);
  const std::string record = encode_retired(up);
  std::lock_guard<std::mutex> lock(mtx_);
  auto &dest = storage_[kRetiredRecordKey];
  if (std::find(dest.begin(), dest.end(), record) == dest.end())
    dest.push_back(record);
}

void Context::record_deprecated_key(const RetiredKeyLine &r) {
  const BlueprintKey *bk = findBlueprintKey(to_upper(r.key), "ALLOWED_KEYS");
  if (bk == nullptr || !bk->isDeprecated())
    throw std::invalid_argument("Context::record_deprecated_key: " + r.key +
                                " is not a deprecated key");
  RetiredKeyLine up = r;
  up.key = to_upper(r.key);
  const std::string record = encode_retired(up);
  std::lock_guard<std::mutex> lock(mtx_);
  auto &dest = storage_[kRetiredRecordKey];
  if (std::find(dest.begin(), dest.end(), record) == dest.end())
    dest.push_back(record);
}

std::vector<RetiredKeyLine> Context::retired_keys() const {
  std::vector<RetiredKeyLine> out;
  std::vector<std::string> records;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    const auto it = storage_.find(kRetiredRecordKey);
    if (it == storage_.end()) return out;
    records = it->second;
  }
  for (const auto &record : records) {
    RetiredKeyLine l = decode_retired(record);
    const BlueprintKey *bk = findBlueprintKey(l.key, "ALLOWED_KEYS");
    if (bk != nullptr && bk->isRetired()) out.push_back(std::move(l));
  }
  return out;
}

std::vector<RetiredKeyLine> Context::deprecated_keys() const {
  std::vector<RetiredKeyLine> out;
  std::vector<std::string> records;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    const auto it = storage_.find(kRetiredRecordKey);
    if (it == storage_.end()) return out;
    records = it->second;
  }
  for (const auto &record : records) {
    RetiredKeyLine l = decode_retired(record);
    const BlueprintKey *bk = findBlueprintKey(l.key, "ALLOWED_KEYS");
    if (bk != nullptr && bk->isDeprecated()) out.push_back(std::move(l));
  }
  return out;
}

void Context::set_key_lines(std::map<std::string, std::vector<KeyLine>> lines) {
  std::map<std::string, std::vector<KeyLine>> up;
  for (auto &[key, l] : lines) up[to_upper(key)] = std::move(l);
  std::lock_guard<std::mutex> lock(mtx_);
  keyLines_ = std::move(up);
}

std::vector<KeyLine> Context::key_lines(const std::string &key) const {
  std::lock_guard<std::mutex> lock(mtx_);
  const auto it = keyLines_.find(to_upper(key));
  return it == keyLines_.end() ? std::vector<KeyLine>() : it->second;
}

void Context::append_block(const ConfigBlock &b) {
  if (b.kind.empty() || b.lines.size() < 2)
    throw std::runtime_error("A structured block needs a header and boundary");
  const auto record = encode_block(b);
  std::lock_guard<std::mutex> lock(mtx_);
  storage_["@BLOCKS"].push_back(record);
}

std::vector<ConfigBlock> Context::blocks(const std::string &kind) const {
  std::lock_guard<std::mutex> lock(mtx_);
  std::vector<ConfigBlock> out;
  const auto it = storage_.find("@BLOCKS");
  if (it == storage_.end()) return out;
  for (const auto &record : it->second) {
    auto b = decode_block(record);
    if (kind.empty() || b.kind == to_upper(kind)) out.push_back(std::move(b));
  }
  return out;
}

void Context::replace_blocks(const std::vector<ConfigBlock> &blocks) {
  std::vector<std::string> records;
  for (const auto &b : blocks) records.push_back(encode_block(b));
  storage_update("@BLOCKS", records);
}

void Context::set_component_cutoffs(const std::vector<double> &radii) {
  std::vector<std::string> values;
  for(double radius:radii) {
    if(!(radius>0) || !std::isfinite(radius))
      throw std::runtime_error("Component spatial radii must be finite and positive");
    values.push_back(format_roundtrip(radius));
  }
  std::lock_guard<std::mutex> lock(mtx_);
  if(values.empty()) storage_.erase("COMPONENT_RCUTMB");
  else storage_["COMPONENT_RCUTMB"]=std::move(values);
  updateInternalKeys();
}

///////////////////////////////////////////////////////////////////////////////
// Constructors / Move
///////////////////////////////////////////////////////////////////////////////
Context::Context()
    : allowedKeys_(BlueprintParser::getKeysMapRef()),
      internalKeys_(BlueprintParser::getInternalKeysMapRef()) {
  // nothing else needed
}

Context::Context(Context &&other) noexcept
    : storage_(std::move(other.storage_)),
      keyLines_(std::move(other.keyLines_)), allowedKeys_(other.allowedKeys_),
      internalKeys_(other.internalKeys_) {
  // The mutex is default-constructed in 'this'.
  // 'other' is left valid but with an empty storage_.
}

Context &Context::operator=(Context &&other) noexcept {
  if (this != &other) {
    // Lock our own mutex (not locking other's for simplicity, assuming
    // single-thread usage).
    std::lock_guard<std::mutex> lock(mtx_);
    storage_ = std::move(other.storage_);
    keyLines_ = std::move(other.keyLines_);
    // The blueprint reference remains the same as in the constructor.
  }
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
// operator()(...)
///////////////////////////////////////////////////////////////////////////////
const std::vector<std::string> &
Context::operator()(const std::string &key) const {
  std::lock_guard<std::mutex> lock(mtx_);
  std::string upKey = to_upper(key);
  auto it = storage_.find(upKey);

  if (it == storage_.end() || it->second.empty()) {
    attemptDefaultInsertion_NoLock(upKey);
    it = storage_.find(upKey);
    if (it == storage_.end() || it->second.empty()) {
      keyNotFoundLogSuggestThrow(key, "operator");
    }
  }
  return it->second;
}

///////////////////////////////////////////////////////////////////////////////
// remove(...)
///////////////////////////////////////////////////////////////////////////////
size_t Context::remove(const std::string &key) {
  std::lock_guard<std::mutex> lock(mtx_);
  std::string upKey = to_upper(key);
  keyLines_.erase(upKey);
  auto it = storage_.find(upKey);
  if (it != storage_.end()) {
    storage_.erase(it);
    updateInternalKeys();
    return 1;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// clear()
///////////////////////////////////////////////////////////////////////////////
void Context::clear() {
  std::lock_guard<std::mutex> lock(mtx_);
  storage_.clear();
  keyLines_.clear();
}

///////////////////////////////////////////////////////////////////////////////
// size(key)
///////////////////////////////////////////////////////////////////////////////
size_t Context::size(const std::string &key) const {
  std::lock_guard<std::mutex> lock(mtx_);
  std::string upKey = to_upper(key);
  auto it = storage_.find(upKey);
  if (it == storage_.end() || it->second.empty()) {
    attemptDefaultInsertion_NoLock(upKey);
    it = storage_.find(upKey);
    if (it == storage_.end() || it->second.empty()) {
      keyNotFoundLogSuggestThrow(key, "size");
    }
  }
  return it->second.size();
}

///////////////////////////////////////////////////////////////////////////////
// size()
///////////////////////////////////////////////////////////////////////////////
size_t Context::size() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return storage_.size();
}

///////////////////////////////////////////////////////////////////////////////
// contains(key)
///////////////////////////////////////////////////////////////////////////////
bool Context::contains(const std::string &key) const {
  std::lock_guard<std::mutex> lock(mtx_);
  std::string upKey = to_upper(key);
  auto it = storage_.find(upKey);
  if (it == storage_.end() || it->second.empty()) {
    attemptDefaultInsertion_NoLock(upKey);
    it = storage_.find(upKey);
  }
  return (it != storage_.end() && !it->second.empty());
}

///////////////////////////////////////////////////////////////////////////////
// has_stored(key) / stored_value(key, n): no default insertion
///////////////////////////////////////////////////////////////////////////////
bool Context::has_stored(const std::string &key) const {
  std::lock_guard<std::mutex> lock(mtx_);
  auto it = storage_.find(to_upper(key));
  return it != storage_.end() && !it->second.empty();
}

std::optional<std::string> Context::stored_value(const std::string &key,
                                                 size_t n) const {
  std::lock_guard<std::mutex> lock(mtx_);
  auto it = storage_.find(to_upper(key));
  if (it == storage_.end() || n >= it->second.size()) return std::nullopt;
  return it->second[n];
}

///////////////////////////////////////////////////////////////////////////////
// write_potential(os, sig_digits) / print_NoLock(...)
///////////////////////////////////////////////////////////////////////////////
void Context::write_potential(std::ostream &os, int sig_digits) const {
  std::lock_guard<std::mutex> lock(mtx_);
  print_NoLock(os, true, sig_digits);
}

namespace {
// True iff any value_type alternative of the key has a DOUBLE position.
// Every other position type (INT, UINT, STRING, BOOL, INDEX_PATTERN)
// either cannot hold a non-integer decimal literal (INT/UINT hold integer
// literals, which reformat_double_token() leaves verbatim) or never does
// in practice (STRING positions of DOUBLE-mixed keys are descriptor,
// element and grid-generator names), so a per-key test is exact for
// potential files.
bool has_double_position(const BlueprintKey &bk) {
  for (const auto &vt : bk.value_type)
    if (vt.find("DOUBLE") != std::string::npos) return true;
  return false;
}

// Whether the writer starts a new line of the key @p bk, written on lines
// of several values (value_N < 0), at its value @p v after the first: for
// a key whose blueprint entry has a `formatting` text (TYPE2B "D2_", TYPEMB
// "DM_"), a value that holds it (a descriptor's name) or the ID of a
// DESCRIPTOR block (a value that starts with '@': TYPEMB @ta). Each TYPEMB
// value schema takes a block's ID alone on its line, so a line that holds a
// block's ID after another value would be read by no command.
bool starts_written_line(const BlueprintKey &bk, const std::string &v) {
  return !bk.formatting.empty() &&
         (v.find(bk.formatting[0]) != std::string::npos ||
          (!v.empty() && v.front() == '@'));
}
} // namespace

std::vector<std::size_t>
Context::written_line_sizes(const std::string &key,
                            const std::vector<std::string> &values) const {
  const BlueprintKey *bi = findBlueprintKey(to_upper(key), "ALLOWED_KEYS");
  if (bi == nullptr || bi->value_N.empty() || bi->value_N[0] == 0 ||
      values.empty())
    return {};
  if (bi->value_N[0] > 0) return std::vector<std::size_t>(values.size(), 1);
  std::vector<std::size_t> sizes{0};
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0 && starts_written_line(*bi, values[i])) sizes.push_back(0);
    ++sizes.back();
  }
  return sizes;
}

void Context::print_NoLock(std::ostream &os, bool potential,
                           int sig_digits) const {
  const size_t max_key_len = 15;
  const std::string space = "   ";
  char buf[kReformatBufferSize];

  // Iterate over all stored key-value pairs.
  for (auto const &pair : storage_) {
    const std::string &key = pair.first;
    const std::vector<std::string> &vals = pair.second;
    if (key == "@BLOCKS") {
      // Component coordinates retain full precision regardless of NUMERIC.
      for (const auto &record : vals)
        for (const auto &line : decode_block(record).lines) os << line << '\n';
      continue;
    }
    // The retired and deprecated lines a parse recorded are not keys:
    // never written.
    if (key == kRetiredRecordKey) continue;

    // NUMERIC is a writer setting, never part of a potential file.
    if (potential && key == "NUMERIC") continue;

    // Check the blueprint so we can interpret value_N.
    const auto bi = findBlueprintKey(key, "ALLOWED_KEYS");
    if (bi == nullptr || bi->value_N.empty()) {
      // Key not in blueprint => skip.
      continue;
    }
    // Take the first element of value_N for printing logic.
    int valN = bi->value_N[0];

    const bool reformat = potential && has_double_position(*bi);
    // A key the loads check exactly (the blueprint's exact_in_potential:
    // the cutoffs and radial grids) is written exactly whatever NUMERIC is:
    // %.{N}g is exact from kRoundTripDigits on, and below it the shortest
    // text that reads back as the same double is written instead.
    const bool exact = reformat && bi->exact_in_potential &&
                       sig_digits < kRoundTripDigits;
    auto put = [&](const std::string &v) {
      if (reformat) {
        const size_t n =
            exact ? reformat_double_token_exact(v, buf, sizeof buf)
                  : reformat_double_token(v, sig_digits, buf, sizeof buf);
        if (n) {
          os.write(buf, static_cast<std::streamsize>(n));
          return;
        }
      }
      os << v;
    };

    if (valN > 0) {
      // Print each value on its own line preceded by key.
      // Example:
      // KEY     val1
      // KEY     val2
      // ...
      for (auto const &v : vals) {
        os << std::left << std::setw(max_key_len) << key << space;
        put(v);
        os << "\n";
      }
    } else if (valN < 0) {
      // Print all values on a single line, preceded by key.
      // Example:
      // KEY     val1     val2     val3
      // Format accordingly to the blueprint.
      os << std::left << std::setw(max_key_len) << key;
      bool doFormatting = false;
      for (auto const &v : vals) {
        // A new line at a value that starts one (written_line_sizes()).
        const bool formatingAvail = starts_written_line(*bi, v);

        if (formatingAvail && doFormatting) {
          os << bi->formatting[1] << std::left << std::setw(max_key_len) << key << space;
        } else {
          os << space;
          doFormatting = true;
        }
        put(v);
      }
      os << "\n";
    }
    // valN == 0 => skip printing entirely.
  }
}

///////////////////////////////////////////////////////////////////////////////
// serialize()
///////////////////////////////////////////////////////////////////////////////
std::vector<char> Context::serialize() const {
  std::lock_guard<std::mutex> lock(mtx_);
  std::ostringstream oss;
  for (auto &entry : storage_) {
    // Format: KEY value1 value2 ...
    oss << entry.first;
    for (size_t i = 0; i < entry.second.size(); ++i) {
      oss << " " << entry.second[i];
    }
    oss << "\n";
  }
  std::string text = oss.str();
  return std::vector<char>(text.begin(), text.end());
}

///////////////////////////////////////////////////////////////////////////////
// deserialize(buffer)
///////////////////////////////////////////////////////////////////////////////
void Context::deserialize(const std::vector<char> &buffer) {
  std::lock_guard<std::mutex> lock(mtx_);
  storage_.clear();
  keyLines_.clear();

  if (buffer.empty()) {
    return; // no data
  }
  std::string data(buffer.begin(), buffer.end());
  std::istringstream iss(data);

  std::string line;
  while (std::getline(iss, line)) {
    if (line.empty()) {
      continue;
    }
    std::istringstream lineStream(line);
    std::vector<std::string> tokens;
    {
      std::string token;
      while (lineStream >> token) {
        tokens.push_back(token);
      }
    }
    if (tokens.empty()) {
      continue;
    }

    // First token is the key
    std::string upKey = to_upper(tokens[0]);

    // Insert tokens as values
    std::vector<std::string> &bucket = storage_[upKey];
    for (size_t i = 1; i < tokens.size(); ++i) {
      /*if (bucket.size() >= maxVals) {*/
      /*  Logger::instance().log(*/
      /*      LogLevel::ERROR,*/
      /*      "Context::deserialize -> Exceeded capacity for key: ", upKey);*/
      /*  throw std::runtime_error("Exceeded capacity for key: " + upKey);*/
      /*}*/
      bucket.push_back(tokens[i]);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// attemptDefaultInsertion_NoLock(upKey)
///////////////////////////////////////////////////////////////////////////////
void Context::attemptDefaultInsertion_NoLock(const std::string &upKey) const {
  const auto bAllowed = findBlueprintKey(upKey, "ALLOWED_KEYS");
  if (bAllowed != nullptr && !bAllowed->default_value.empty()) {
    storage_.insert(std::make_pair(upKey, bAllowed->default_value));
    return;
  }
  const auto bInternal = findBlueprintKey(upKey, "INTERNAL_KEYS");
  if (bInternal != nullptr && !bInternal->default_value.empty()) {
    storage_.insert(std::make_pair(upKey, bInternal->default_value));
    return;
  }
  return;
}
bool Context::operator==(const Context &other) const {
  /* Lock BOTH contexts for the comparison: storage_ is mutated under mtx_
   * even by const getters (lazy blueprint-default insertion), so an unlocked
   * read here raced against concurrent get()/contains() calls. scoped_lock
   * acquires both mutexes deadlock-free; comparing an object with itself
   * must not lock the same mutex twice. (This replaced getMapRef(), whose
   * unsynchronized reference return had exactly this race — removed.) */
  if (this == &other) return true;
  std::scoped_lock lock(mtx_, other.mtx_);
  return storage_ == other.storage_;
}

//-------------------------------------------------------------
// findBlueprintKey(upKey):
// returns pointer to either allowed item or internal item
//-------------------------------------------------------------
const BlueprintKey *
Context::findBlueprintKey(const std::string &upKey,
                           const std::string &xKeys) const {
  // print size of allowedKeys_ and internalKeys_
  if (xKeys == "ALLOWED_KEYS") {
    auto itA = allowedKeys_.find(upKey);
    if (itA != allowedKeys_.end()) {
      return &(itA->second);
    } else {
      return nullptr;
    }
  }

  if (xKeys == "INTERNAL_KEYS") {
    auto itI = internalKeys_.find(upKey);
    if (itI != internalKeys_.end()) {
      return &(itI->second);
    } else {
      return nullptr;
    }
  }
  // Unknown keyMap
  return nullptr;
}

void Context::keyNotFoundLogSuggestThrow(const std::string &key,
    const std::string &method) const {
  // A retired key is never stored: say what replaces it.
  const auto allowed = allowedKeys_.find(to_upper(key));
  const bool isAllowed = allowed != allowedKeys_.end();
  if (isAllowed && allowed->second.isRetired())
    throwRetiredKey(allowed->first, method);
  Logger::instance().log(LogLevel::ERROR,
      "Context::" + method + "() -> Key not found: ", key);

  if (isAllowed) {
    throw std::runtime_error("Key not found in the Context object: " + key);
  } else {
    std::string suggested = BlueprintParser::findClosestKey(key, allowedKeys_);
    if (!suggested.empty()) {
      Logger::instance().log(LogLevel::ERROR, "  closest key: ", suggested);
    }
    throw std::runtime_error("Key not allowed: " + key);
  }
}

void Context::throwRetiredKey(const std::string &upKey,
                              const std::string &method) const {
  const auto it = allowedKeys_.find(upKey);
  const std::string text =
      it == allowedKeys_.end() ? upKey + " is retired"
                               : retired_key_text(upKey, it->second);
  Logger::instance().log(LogLevel::ERROR,
                         "Context::" + method + "() -> " + text);
  throw std::runtime_error(text);
}

} // namespace core
} // namespace tadah
