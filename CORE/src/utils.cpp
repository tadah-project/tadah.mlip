#include <tadah/core/utils.h>
#include <tadah/core/context.h>
#include <tadah/core/number_format.h>
#include <stdexcept>
#include <set>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <charconv>

namespace tadah {
namespace core {

v_type geomspace(double start, double stop, int num, double base) {
    // Disallow any non-positive start to avoid std::log of negative or zero
    if (start <= 0.0) {
        throw std::runtime_error("geomspace() start must be > 0");
    }

    // existing code
    double realStart = std::pow(base, std::log(start));
    double realBase  = std::pow(base, (std::log(stop)-std::log(start)) / (num - 1));

    v_type retval;
    retval.reserve(num);
    std::generate_n(std::back_inserter(retval), num, Logspace<double>(realStart, realBase));
    return retval;
}
v_type linspace(double start, double end, int num) {

  v_type ls(num);

  if (num == 0) { return ls; }
  if (num == 1)
    {
      ls[0] = start;
      return ls;
    }

  double delta = (end - start) / (num - 1);

  for(int i=0; i < num-1; ++i)
    {
      //ls.push_back(start + delta * i);
      ls[i] = start + delta * i;
    }
  ls[num-1] = end;
  return ls;
}


size_t number_of_digits (size_t i)
{
    return i > 0 ? (int) log10 ((double) i) + 1 : 1;
}
size_t max_number(size_t n) {
    size_t val = 0;
    if (n == 0) return 0;
    for (size_t i=0; i<n; ++i) val+= 9*std::pow(10,i);
    return val;
}

// convert vector to string
std::string vec_to_string(std::vector<double> vec,
        std::string delim) {
    std::stringstream ss;
    for (auto &v:vec) ss << std::setprecision(15)
        << v << delim;
    return ss.str().substr(0,ss.str().size()-delim.size()); // remove last delim
}

//std::vector<double> calc_rho(Structure &st) {
//    // Computes density according to Ta2 by Ravelo
//    double p=5.9913;
//    double q=8.0;
//    double rho0=0.074870;
//    double rc=5.5819;
//    double rc_sq=rc*rc;
//    double rcp=pow(rc,p);
//    double a0=3.304;
//    double r0=sqrt(3)*a0/2;
//    double r0p=pow(r0,p);
//    // assume full NN list
//    std::vector<double> rho(st.natoms(),0);
//    for (size_t i=0; i<st.natoms(); ++i) {
//        const Atom &a1 = st.atoms[i];
//        for (size_t jj=0; jj<st.nn_size(i); ++jj) {
//            const Vec3d &a2pos = st.nn_pos(i,jj);
//            Vec3d delij = a1.position - a2pos;
//            double rij_sq = delij.transpose() * delij;
//            if (rij_sq > rc_sq) continue;
//            double r=sqrt(rij_sq);
//            rho[i]+=rho0*pow((rcp-pow(r,p))/(rcp-r0p),q);
//        }
//    }
//    return rho;
//}
std::string to_upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), ::toupper);
  return s;
}
std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  return s;
}
bool is_blank_line(const std::string& line) {
    for (char c : line) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}


std::vector<size_t> divide_into_parts(size_t number, int n) {
  if (n <= 0) {
    throw std::runtime_error("divide_into_parts called with n=0");
  }
  std::vector<size_t> parts(n, number / n);
  size_t remainder = number % n;
  for (size_t i = 0; i < remainder; ++i) {
    parts[i] += 1;
  }
  return parts;
}

std::vector<size_t> divide_by_percentage(size_t number, const std::vector<size_t>& percentages) {
    std::vector<size_t> parts(percentages.size(), 0);
    size_t totalAssigned = 0;

    for (size_t i = 0; i < percentages.size(); ++i) {
        parts[i] = number * percentages[i] / 100;
        totalAssigned += parts[i];
    }

    size_t remainder = number - totalAssigned;

    // Distribute remaining amount due to rounding
    for (size_t i = 0; remainder > 0; ++i, --remainder) {
        parts[i]++;
    }

    return parts;
}
bool is_integer(double d) {
    return std::floor(d) == d;
}
bool is_integer(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}
bool is_number(const std::string& str) {
    // std::stod semantics over the whole token, but an underflowing literal
    // (a subnormal such as "1e-310", which the lossless writers produce) is
    // a number; overflow is not.
    double v = 0.0;
    return parse_number(str, v);
}

std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t first = str.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(whitespace);
    return str.substr(first, last - first + 1);
}

void trimInPlace(std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t first = str.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        str.clear();
        return;
    }
    size_t last = str.find_last_not_of(whitespace);
    str = str.substr(first, last - first + 1);
}

std::string join(const std::vector<std::string>& vec) {
    std::ostringstream oss;
    if (!vec.empty()) {
        oss << vec[0];
        for (size_t i = 1; i < vec.size(); ++i) {
            oss << ' ' << vec[i];
        }
    }
    return oss.str();
}

namespace {
// The expansion of parse_indices_strict: subtract before incrementing so a
// range ending at SIZE_MAX cannot wrap, and a zero stride cannot hang it.
void append_index_range(size_t start, size_t end, size_t step,
                        std::vector<size_t>& result, std::set<size_t>& seen,
                        bool& duplicates) {
  if (step == 0) throw std::runtime_error("Index stride must be positive");
  if (start > end) return;
  for (size_t i = start;;) {
    if (seen.insert(i).second) result.push_back(i);
    else duplicates = true;
    if (step > end - i) break;
    i += step;
  }
}
}

std::vector<size_t> parse_indices_strict(const std::string& input) {
  std::vector<size_t> result;
  std::set<size_t> seen;
  bool duplicates = false;
  auto number = [](const std::string& text) {
    const std::string s = trim(text);
    size_t value = 0;
    const auto parsed = std::from_chars(s.data(), s.data() + s.size(), value);
    if (s.empty() || parsed.ec != std::errc{} || parsed.ptr != s.data() + s.size())
      throw std::runtime_error("Invalid index number '" + s + "'");
    return value;
  };
  size_t pos = 0;
  do {
    const auto comma = input.find(',', pos);
    const auto segment = trim(input.substr(pos, comma == std::string::npos
                                                  ? comma : comma - pos));
    const auto dash = segment.find('-');
    const auto colon = segment.find(':');
    size_t start, end, step = 1;
    if (dash == std::string::npos) {
      start = end = number(segment);
    } else {
      start = number(segment.substr(0, dash));
      end = number(segment.substr(dash + 1, colon == std::string::npos
                                               ? colon : colon - dash - 1));
      if (colon != std::string::npos) step = number(segment.substr(colon + 1));
      if (start > end) throw std::runtime_error("Descending index range '" + segment + "'");
    }
    append_index_range(start, end, step, result, seen, duplicates);
    if (duplicates) throw std::runtime_error("Duplicate index in '" + input + "'");
    if (comma == std::string::npos) break;
    pos = comma + 1;
  } while (true);
  return result;
}

std::vector<size_t> parse_dataset_indices(const std::vector<std::string>& values,
                                          size_t total, const std::string& who) {
  return parse_one_based_indices(values, total, who,
                                 "the dataset has " + std::to_string(total) +
                                     " structures");
}

namespace {

/* One entry of a strict index list ('3', '2-9', '1-9:2'): the indices
 * first, first + step, ... up to last (first <= last, step >= 1). */
struct IndexEntry {
  std::string text;
  size_t first = 0;
  size_t last = 0;
  size_t step = 1;
};

/* The strict grammar of parse_one_based_indices() and parse_fixindex(): the
 * values form one list (joined, without the spaces around '-' and ':'),
 * entries separated by commas or whitespace, each 'i', 'a-b' or 'a-b:step'.
 * Calls visit(entry) for each entry in the order given, before the next is
 * read, so a caller's own check of an entry (its bounds) comes in that
 * order too. An empty entry, a token that is not an index, a descending
 * range and a step of 0 are refused: "<who>: index '5-2' is a descending
 * range<usage>". Nothing is expanded. */
template <typename Visit>
void for_each_index_entry(const std::vector<std::string>& values,
                          const std::string& who, const std::string& usage,
                          Visit&& visit) {
  std::string list;
  {
    const std::string joined = join(values);
    for (size_t i = 0; i < joined.size(); ++i) {
      const char c = joined[i];
      if (std::isspace(static_cast<unsigned char>(c))) {
        const size_t prev = list.find_last_not_of(" \t\r\n\v\f");
        const size_t next = joined.find_first_not_of(" \t\r\n\v\f", i);
        if ((prev != std::string::npos && (list[prev] == '-' || list[prev] == ':')) ||
            (next != std::string::npos && (joined[next] == '-' || joined[next] == ':')))
          continue;
      }
      list += c;
    }
  }
  auto whole_number = [](const std::string& t, size_t& value) {
    const auto r = std::from_chars(t.data(), t.data() + t.size(), value);
    return !t.empty() && r.ec == std::errc{} && r.ptr == t.data() + t.size();
  };
  size_t pos = 0;
  while (true) {
    const size_t comma = list.find(',', pos);
    const std::string segment =
        list.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
    if (trim(segment).empty())
      throw std::runtime_error(who + ": the index list '" + trim(join(values)) +
                               "' has an empty entry" + usage);
    std::istringstream tokens(segment);
    for (std::string token; tokens >> token;) {
      const auto bad = [&](const std::string& why) {
        return std::runtime_error(who + ": index '" + token + "' " + why + usage);
      };
      IndexEntry e;
      e.text = token;
      const size_t dash = token.find('-');
      if (dash == std::string::npos) {
        if (!whole_number(token, e.first)) throw bad("is not an index");
        e.last = e.first;
      } else {
        const size_t colon = token.find(':', dash);
        if (!whole_number(token.substr(0, dash), e.first) ||
            !whole_number(token.substr(dash + 1, colon == std::string::npos
                                                     ? std::string::npos
                                                     : colon - dash - 1), e.last) ||
            (colon != std::string::npos && !whole_number(token.substr(colon + 1), e.step)))
          throw bad("is not an index range");
        if (e.first > e.last) throw bad("is a descending range");
        if (e.step == 0) throw bad("has a step of 0");
      }
      visit(static_cast<const IndexEntry&>(e));
    }
    if (comma == std::string::npos) break;
    pos = comma + 1;
  }
}

/* " (in '2-9')" when index i is not the whole entry, "" when it is. */
std::string in_entry(size_t i, const IndexEntry& e) {
  return e.text == std::to_string(i) ? "" : " (in '" + e.text + "')";
}

}  // namespace

std::vector<size_t> parse_one_based_indices(const std::vector<std::string>& values,
                                            size_t total, const std::string& who,
                                            const std::string& counted) {
  const std::string usage = " (use i, a-b or a-b:step, 1-based, separated by commas)";
  std::vector<size_t> out;
  std::set<size_t> seen;
  for_each_index_entry(values, who, usage, [&](const IndexEntry& e) {
    // The bounds first: a range is expanded only within 1 ... total.
    for (const size_t i : {e.first, e.last})
      if (i == 0 || i > total)
        throw std::runtime_error(who + ": index " + std::to_string(i) +
                                 in_entry(i, e) + " is outside 1-" +
                                 std::to_string(total) + " (" + counted + ")");
    for (size_t i = e.first;; i += e.step) {
      if (seen.insert(i).second) out.push_back(i);
      if (e.last - i < e.step) break;
    }
  });
  return out;
}

std::vector<size_t> parse_fixindex(const Context& ctx,
                                   std::optional<size_t> n_weights) {
  const bool has_fi = ctx.has_stored("FIXINDEX");
  const bool has_fw = ctx.has_stored("FIXWEIGHT");
  if (has_fi != has_fw)
    throw std::runtime_error(
        "FIXINDEX requires FIXWEIGHT (and vice versa). Provide both keys with "
        "matching lengths; the i-th FIXWEIGHT value pins the weight named by "
        "the i-th FIXINDEX entry. There is no auto-seeding from WEIGHTS.");
  if (!has_fi) return {};

  const std::vector<std::string>& values = ctx("FIXINDEX");
  const std::string list = trim(join(values));
  const std::string usage =
      " (FIXINDEX takes 1-based weight indices i, a-b or a-b:step, separated "
      "by commas or spaces)";
  auto plural = [](size_t n, const std::string& one, const std::string& many) {
    return std::to_string(n) + " " + (n == 1 ? one : many);
  };

  // The entries, each checked against the bounds and counted, not expanded.
  std::vector<IndexEntry> entries;
  size_t count = 0;
  bool count_overflows = false;
  for_each_index_entry(values, "FIXINDEX", usage, [&](const IndexEntry& e) {
    for (const size_t i : {e.first, e.last}) {
      if (n_weights && (i == 0 || i > *n_weights))
        throw std::runtime_error(
            "FIXINDEX: index " + std::to_string(i) + in_entry(i, e) +
            " is outside 1-" + std::to_string(*n_weights) + " (the model has " +
            plural(*n_weights, "weight", "weights") + ")");
      if (i == 0)
        throw std::runtime_error("FIXINDEX: index 0" + in_entry(0, e) +
                                 " is not a model weight (the weights are "
                                 "numbered from 1)");
    }
    // first >= 1, so this is at most SIZE_MAX.
    const size_t n = (e.last - e.first) / e.step + 1;
    if (count_overflows || n > std::numeric_limits<size_t>::max() - count)
      count_overflows = true;
    else
      count += n;
    entries.push_back(e);
  });

  // FIXWEIGHT pairs by position, so each weight is named once and there are
  // as many indices as values. The first n_values + 1 indices are expanded,
  // in order, and a weight named twice among them is refused as such: so is
  // every list with a repeat that names no more distinct weights than
  // FIXWEIGHT has values (n_values + 1 of its indices name at most n_values
  // weights), '13,13,14' with two values, whose counts would agree without
  // the repeat, as with three. No range is expanded past them: then the
  // count is compared.
  const size_t n_values = ctx.size("FIXWEIGHT");
  std::vector<size_t> out;
  std::set<size_t> seen;
  for (const IndexEntry& e : entries) {
    for (size_t i = e.first; out.size() <= n_values; i += e.step) {
      if (!seen.insert(i).second)
        throw std::runtime_error(
            "FIXINDEX " + list + " names coefficient " + std::to_string(i) +
            " twice; each FIXWEIGHT value fixes one coefficient (the i-th "
            "value the coefficient of the i-th index), so FIXINDEX names each "
            "coefficient once.");
      out.push_back(i);
      if (e.last - i < e.step) break;
    }
    if (out.size() > n_values) break;
  }
  if (count_overflows || count != n_values)
    throw std::runtime_error(
        "FIXINDEX " + list + " has " +
        (count_overflows
             ? "more than " + std::to_string(std::numeric_limits<size_t>::max()) +
                   " indices"
             : plural(count, "index", "indices")) +
        " but FIXWEIGHT has " + plural(n_values, "value", "values") +
        "; they must pair 1:1 (the i-th FIXWEIGHT value pins the weight of "
        "the i-th FIXINDEX index).");
  // count == n_values: every index expanded, each weight once.
  return out;
}

void reverse_vector(t_type& vec, const std::vector<size_t>& original_map) {
    t_type temp_vec = vec;
    for (size_t i = 0; i < original_map.size(); ++i) {
        vec[original_map[i]] = temp_vec[i];
    }
}

std::vector<histogram_bin> generate_2d_histogram(
    const std::vector<std::pair<double,double>>& data_2d,
    int x_bins,
    int y_bins)
{
    if (data_2d.empty()) {
        return {};
    }

    // Initialize minimum and maximum values
    double x_min = std::numeric_limits<double>::max();
    double x_max = std::numeric_limits<double>::lowest();
    double y_min = std::numeric_limits<double>::max();
    double y_max = std::numeric_limits<double>::lowest();

    // Find min and max values along x and y axes from the data
    for (const auto& point : data_2d) {
        double x_value = point.first;
        double y_value = point.second;

        if (x_value < x_min) x_min = x_value;
        if (x_value > x_max) x_max = x_value;
        if (y_value < y_min) y_min = y_value;
        if (y_value > y_max) y_max = y_value;
    }

    // Check if min and max values are valid
    if (x_min >= x_max || y_min >= y_max) {
        // Invalid data range
        return {};
    }

    // Initialize the histogram bins with zero counts
    std::vector<std::vector<size_t>> histogram(x_bins, std::vector<size_t>(y_bins, 0));

    // Calculate the width of each bin along x and y axes
    double x_bin_width = (x_max - x_min) / x_bins;
    double y_bin_width = (y_max - y_min) / y_bins;

    // Iterate over the data points and increment the corresponding histogram bins
    for (const auto& point : data_2d) {
        double x_value = point.first;
        double y_value = point.second;

        // Calculate the bin indices for the current data point
        int x_bin = static_cast<int>((x_value - x_min) / x_bin_width);
        int y_bin = static_cast<int>((y_value - y_min) / y_bin_width);

        // Handle the edge case where the data point is exactly at the maximum value
        if (x_bin == x_bins) x_bin = x_bins - 1;
        if (y_bin == y_bins) y_bin = y_bins - 1;

        // Increment the count in the appropriate bin
        ++histogram[x_bin][y_bin];
    }

    // Prepare the output vector containing histogram bins with centers and counts
    std::vector<histogram_bin> histogram_bins;
    for (int ix = 0; ix < x_bins; ++ix) {
        double x_center = x_min + (ix + 0.5) * x_bin_width;
        for (int iy = 0; iy < y_bins; ++iy) {
            double y_center = y_min + (iy + 0.5) * y_bin_width;
            size_t count = histogram[ix][iy];
            histogram_bins.push_back(std::make_tuple(x_center, y_center, count));
        }
    }

    return histogram_bins;
}

std::string removeFirstWord(const std::string &input, const std::string &knownWord) {
  // Check whether 'input' starts with 'knownWord'
  if (input.compare(0, knownWord.size(), knownWord) == 0) {
    // Compute new starting position after removing the known word
    // plus any subsequent whitespace
    std::size_t startPos = knownWord.size();
    while (startPos < input.size() && std::isspace(static_cast<unsigned char>(input[startPos]))) {
      ++startPos;
    }
    // Return the substring excluding the known word and trailing spaces
    return input.substr(startPos);
  }
  // Return original string if the word does not appear at the start
  return input;
}

std::string concat(const std::vector<std::string>& vec, const std::string& delim) {
  std::string result;
  for (const auto& str : vec) {
    if (!result.empty()) {
      result += delim;
    }
    result += str;
  }
  return result;
}

std::string wrapText(const std::string &text,
    std::size_t maxWidth,  bool preserveWords,
    const std::string &linePadding) {
  // Sanity check: ensure maxWidth is not zero
  if (maxWidth == 0) {
    return text;  // or throw an exception if preferred
  }

  // ---------------------------------------------------------------------------
  // Helper function for chunk-based wrapping (ignores word boundaries).
  // ---------------------------------------------------------------------------
  auto chunkWrapLine = [&](const std::string &line) {
    std::vector<std::string> segments;
    if (line.empty()) {
      // Produce one empty segment for empty lines
      segments.emplace_back();
      return segments;
    }

    std::size_t startPos = 0;
    // Continue until the entire line is processed
    while (startPos < line.size()) {
      // Determine remaining characters in the line
      std::size_t remaining = line.size() - startPos;
      // Number of characters to copy for this segment
      std::size_t chunkSize = (remaining < maxWidth) ? remaining : maxWidth;

      segments.push_back(line.substr(startPos, chunkSize));
      startPos += chunkSize;
    }
    return segments;
  };

  // ---------------------------------------------------------------------------
  // Helper function for word-based wrapping (preserves word boundaries).
  // ---------------------------------------------------------------------------
  auto wordWrapLine = [&](const std::string &line) {
    std::vector<std::string> segments;

    if (line.empty()) {
      // Produce one empty segment for empty lines
      segments.emplace_back();
      return segments;
    }

    // Split the line into words using a stream
    std::istringstream iss(line);
    std::string word;
    
    std::string currentLine;
    while (iss >> word) {
      // If adding this word (plus a preceding space if needed) exceeds maxWidth,
      // then finalize the current line and start a new one.
      if (currentLine.empty()) {
        // If currentLine is empty, this is the first word on a new line.
        if (word.size() > maxWidth) {
          // If a single word itself exceeds maxWidth, place it alone on a line.
          segments.push_back(word);
        } else {
          // Start a new line with this word
          currentLine = word;
        }
      } else {
        // If not empty, check if appending " word" would exceed maxWidth
        if (currentLine.size() + 1 + word.size() > maxWidth) {
          // Finalize currentLine and start a new line
          segments.push_back(currentLine);
          // If the new word exceeds maxWidth alone, put it on its own line
          if (word.size() > maxWidth) {
            segments.push_back(word);
          } else {
            currentLine = word;
          }
        } else {
          // Append the word with a preceding space
          currentLine += " " + word;
        }
      }
    }

    // Push the last accumulated line if not empty
    if (!currentLine.empty()) {
      segments.push_back(currentLine);
    }

    return segments;
  };

  // ---------------------------------------------------------------------------
  // Process the original text line by line, then wrap each line.
  // For each line from the input, the first segment is not padded, 
  // and subsequent "broken" segments are padded.
  // ---------------------------------------------------------------------------
  std::stringstream result;
  std::size_t start = 0;
  while (start <= text.size()) {
    // Locate the next newline
    std::size_t newlinePos = text.find('\n', start);
    bool foundNewline = (newlinePos != std::string::npos);

    // Extract the current logical line (up to newline or end of text)
    std::string currentLine = foundNewline
      ? text.substr(start, newlinePos - start)
      : text.substr(start);

    // Wrap the line using whichever mode is selected
    std::vector<std::string> wrappedLines = preserveWords
      ? wordWrapLine(currentLine)
      : chunkWrapLine(currentLine);

    // Write each wrapped segment to the result
    // - No padding for the first segment
    // - Padding for subsequent segments if any
    for (std::size_t i = 0; i < wrappedLines.size(); ++i) {
      if (i == 0) {
        // First ("original") segment from this line has no padding
        result << wrappedLines[i] << '\n';
      } else {
        // "Broken" line: apply padding
        result << linePadding << wrappedLines[i] << '\n';
      }
    }

    // Advance start position past the newline if one was found
    if (foundNewline) {
      start = newlinePos + 1;
    } else {
      // No more newlines -> done
      break;
    }
  }

  return result.str();
}
} // namespace core
} // namespace tadah
