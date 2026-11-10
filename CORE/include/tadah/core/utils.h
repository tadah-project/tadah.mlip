#ifndef TADAH_CORE_UTILS_H
#define TADAH_CORE_UTILS_H

#include <tadah/core/core_types.h>
#include <tadah/core/number_format.h>

#include <iterator>
#include <algorithm>
#include <vector>
#include <cmath>
#include <charconv>
#include <sstream>
#include <iomanip>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <cstddef>
#include <optional>
#include <utility>

namespace tadah {
namespace core {

class Context;

/** Vector printing to streams */
template <typename T>
std::ostream& operator<< (std::ostream& os, const std::vector<T>& v) {
    if ( !v.empty() ) {
        //os << '[';
        std::copy (v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
        //os << "\b\b]\n";
    }
    return os;
}

/**
 * @class Logspace
 * @brief A generator (functor) for producing elements of a geometric progression.
 *
 * This class stores a current value and a base ratio. Each invocation of @c operator()
 * returns the current value and then multiplies it by the base ratio, advancing
 * the sequence in a geometric progression.
 *
 * @tparam T A numeric type (e.g., @c float, @c double, etc.).
 */
template<typename T>
class Logspace {
    private:
        T curValue, base;

    public:
        Logspace(T first, T base) : curValue(first), base(base) {}

        T operator()() {
            T retval = curValue;
            curValue *= base;
            return retval;
        }
};

/**
 * @brief Generates a geometric progression of @p num points between @p start and @p stop.
 *
 * The progression is computed by:
 *   1. Converting @p start into \c realStart = base^(ln(start)).
 *   2. Calculating the common ratio \c realBase = base^((ln(stop) - ln(start)) / (num - 1)).
 *   3. Generating @p num values by repeatedly multiplying \c realStart by \c realBase.
 *
 * @param[in] start Beginning value of the sequence (must be > 0).
 * @param[in] stop  Ending value of the sequence (should be > 0).
 * @param[in] num   Number of elements in the sequence.
 * @param[in] base  The base used to transform and compute the ratio (commonly @c exp(1)).
 *
 * @return A vector (`v_type`) of @p num values in geometric progression ranging
 *         approximately from @p start to @p stop.
 *
 * @throws std::runtime_error if @p start <= 0 (since ln(start) would be invalid).
 *
 * ### Example
 * @code{.cpp}
 * // Suppose you want a geometric progression from 1.0 to 100.0 in 5 steps:
 * // The ratio will be chosen so that the final value is 100.
 *
 * v_type seq = geomspace(1.0, 100.0, 5, std::exp(1));
 * // seq might contain something like [1.0, ~3.162, ~10, ~31.622, 100.0]
 * @endcode
 */
v_type geomspace(double start, double stop, int num, double base);

v_type linspace(double start, double end, int num);


/**
 * @brief Convert a value to string (no precision specified).
 *
 * @tparam T Any type supporting operator<< to std::ostream.
 * @param value The value to convert.
 * @return std::string Representation of the value.
 */
template <typename T>
inline std::string to_string(const T& value)
{
    std::stringstream ss;
    ss << value; 
    return ss.str();
}

/**
 * @brief Convert a value to string with given precision (for numeric types).
 *
 * Fixed notation with @p n decimals: for human-facing text only (logs,
 * messages). Text that is parsed back into a calculation must use
 * to_string_lossless().
 *
 * @tparam T Any type supporting operator<< to std::ostream.
 * @param value The value to convert.
 * @param n   Decimal precision (for floating types).
 * @return std::string Representation of the value with precision n.
 */
template <typename T>
inline std::string to_string(const T& value, int n)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(n) << value;
    return ss.str();
}

/**
 * @brief Specialization for bool (no precision).
 *
 * Writes "true" or "false" instead of "1" or "0".
 */
template <>
inline std::string to_string<bool>(const bool& value)
{
    return value ? "true" : "false";
}

/**
 * @brief Specialization for bool (with precision).
 *
 * Ignores the precision parameter and writes "true" or "false".
 */
template <>
inline std::string to_string<bool>(const bool& value, int /*n*/)
{
    return value ? "true" : "false";
}

namespace detail {
template <typename T, typename = void>
struct has_index_access : std::false_type {};
template <typename T>
struct has_index_access<T, std::void_t<
    decltype(std::declval<const T&>().size()),
    decltype(std::declval<const T&>()[std::size_t{0}])>> : std::true_type {};

template <typename T, typename = void>
struct has_begin_end : std::false_type {};
template <typename T>
struct has_begin_end<T, std::void_t<
    decltype(std::begin(std::declval<const T&>())),
    decltype(std::end(std::declval<const T&>()))>> : std::true_type {};

template <typename T>
constexpr bool is_narrow_char_v = std::is_same_v<T, char> ||
    std::is_same_v<T, signed char> || std::is_same_v<T, unsigned char>;
} // namespace detail

/**
 * @brief Lossless text for a value that will be parsed back into a
 * calculation (Context storage).
 *
 * Floating-point values use the shortest round-trip form
 * (format_roundtrip(); a float is widened to double exactly, a long double
 * uses its own shortest form), so parsing the text gives the identical
 * value. Integers use std::to_string, bool "true"/"false", characters and
 * strings are copied verbatim. Ranges (anything with size() and
 * operator[], e.g. std::vector or aed_type, or with begin()/end()) are
 * converted element-wise and joined with single spaces. Any other type is
 * streamed with 17 significant digits.
 *
 * Unlike to_string(value, n) this never limits precision; use to_string()
 * only for human-facing text (logs, messages).
 */
template <typename T>
std::string to_string_lossless(const T& value)
{
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<U, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_same_v<U, long double>) {
        char buf[64];
        const std::to_chars_result r = std::to_chars(buf, buf + sizeof buf, value);
        return std::string(buf, r.ptr);
    } else if constexpr (std::is_floating_point_v<U>) {
        return format_roundtrip(static_cast<double>(value));
    } else if constexpr (detail::is_narrow_char_v<U>) {
        return std::string(1, static_cast<char>(value));
    } else if constexpr (std::is_integral_v<U>) {
        return std::to_string(value);
    } else if constexpr (std::is_convertible_v<const U&, std::string_view>) {
        return std::string(std::string_view(value));
    } else if constexpr (detail::has_index_access<U>::value) {
        std::string out;
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (i) out += ' ';
            out += to_string_lossless(value[i]);
        }
        return out;
    } else if constexpr (detail::has_begin_end<U>::value) {
        std::string out;
        bool first = true;
        for (const auto& v : value) {
            if (!first) out += ' ';
            out += to_string_lossless(v);
            first = false;
        }
        return out;
    } else {
        std::ostringstream ss;
        ss << std::setprecision(kRoundTripDigits) << value;
        return ss.str();
    }
}


std::string to_upper(std::string s);
std::string to_lower(std::string s);

template <typename F>
double central_difference(F f, double val, double h) {
    return (f(val+h)-f(val-h))/(2.0*h);
}
size_t number_of_digits (size_t i);
size_t max_number(size_t i);    // max_number(6) -> 999999

template<typename T>
T random(T range_from, T range_to) {
    std::random_device                  rand_dev;
    std::mt19937                        generator(rand_dev());
    std::uniform_real_distribution<T>    distr(range_from, range_to);
    return distr(generator);
}

/** Find element in std::vector and return its index.
 *
 * If not found returns -1.
 */
template <typename T>
int get_index(std::vector<T> v, T K) {
    auto it = find(v.begin(), v.end(), K);
    int index=-1;
    if (it != v.end()) {
        index = it - v.begin();
    }
    return index;
}
std::string vec_to_string(std::vector<double> vec,
        std::string delim=" ");

/** Print C++ parameter pack
 *
 * Example:
 *  print_pp(std::cout,args...);
 */
template <typename Arg, typename... Args>
void print_pp(std::ostream& out, Arg&& arg, Args&&... args)
{
    out << std::forward<Arg>(arg);
    using expander = int[];
    (void)expander{0, (void(out << ',' << std::forward<Args>(args)), 0)...};
}

//std::vector<double> calc_rho(Structure &st);

bool is_blank_line(const std::string& line);

/**
 * @brief Checks if a line is blank (empty or only contains whitespace).
 * 
 * @param line The string to check.
 * @return true if the line is blank, false otherwise.
 */
bool is_blank_line(const std::string& line);

/**
 * @brief Divides a number into approximately equal parts.
 * 
 * @param number The number to divide.
 * @param n The number of parts to divide into.
 * @return A vector containing the parts.
 */
std::vector<size_t> divide_into_parts(size_t number, int n);

/**
 * @brief Divides a number according to specified percentages.
 * 
 * @param number The total number to divide.
 * @param percentages A vector of percentages to divide by.
 * @return A vector containing the divided parts based on percentages.
 */
std::vector<size_t> divide_by_percentage(size_t number, const std::vector<size_t>& percentages);

/**
 * @brief Checks if a double represents an integer.
 * 
 * @param d The double to check.
 * @return true if the double represents an integer.
 */
bool is_integer(double d);

/**
 * @brief Checks if a string represents an integer.
 * 
 * @param s The string to check.
 * @return true if the string is an integer.
 */
bool is_integer(const std::string& s);

/**
 * @brief Checks if a string represents a number.
 *
 * True iff the whole string converts as std::stod would (leading
 * whitespace, decimal/hex, inf/nan) without overflowing; an underflowing
 * literal (subnormal) counts as a number. See parse_number().
 *
 * @param s The string to check.
 * @return true if the string is numeric, false otherwise.
 */
bool is_number(const std::string& s);

/**
 * @brief Trims leading and trailing spaces from a string.
 * 
 * @param str The string to trim.
 * @return A new string with leading and trailing spaces removed.
 */
std::string trim(const std::string& str);

/**
 * @brief Trims leading and trailing spaces from a string in place.
 * 
 * @param str The string to trim.
 */
void trimInPlace(std::string& str);

/**
 * @brief Combines a vector of strings into a single string, with each element separated by a space.
 * 
 * @param vec The vector of strings to join.
 * @return A single string containing all elements of the vector, separated by spaces.
 */
std::string join(const std::vector<std::string>& vec);

/** Strict list/range/stride parser. Rejects malformed segments, duplicates,
 * descending ranges, zero strides and integer overflow. Zero is a valid
 * value here; one-based callers must reject it at their boundary. */
std::vector<size_t> parse_indices_strict(const std::string& input);

/**
 * @brief The 1-based structure indices of a dataset command (tadah data
 * sample, data balance and data write -i), checked.
 *
 * parse_one_based_indices() with @p counted "the dataset has <total>
 * structures": an index outside the dataset is refused with
 * "<who>: index 11 is outside 1-10 (the dataset has 10 structures)".
 */
std::vector<size_t> parse_dataset_indices(const std::vector<std::string>& values,
                                          size_t total, const std::string& who);

/**
 * @brief A checked list of 1-based indices of @p total items: structures
 * (parse_dataset_indices(), tadah data print and analysis descriptor -i) or
 * basis functions (tadah analysis bfunc -i).
 *
 * The values form one list, as the command line and a configuration file
 * give them ('1, 3' arrives as the values '1,' and '3'; '-i 1 3' as '1' and
 * '3'):
 * entries separated by commas or whitespace, each 'i', 'a-b' or 'a-b:step',
 * with spaces allowed around '-' and ':'. Indices are returned in the given
 * order, repeated ones removed.
 *
 * Refused, naming the text and prefixed with @p who: an empty entry ('1,,2',
 * a leading or trailing comma), a token that is not an index, a descending
 * range, a step of 0, and an index outside 1 ... @p total, whose message
 * ends with @p counted in parentheses, what the indices count
 * ("tadah analysis bfunc: index 9 (in '2-9') is outside 1-4 (the D2_Blip
 * basis has 4 functions)"). The bounds are checked before a range is
 * expanded, so a range past the end is refused at once.
 */
std::vector<size_t> parse_one_based_indices(const std::vector<std::string>& values,
                                            size_t total, const std::string& who,
                                            const std::string& counted);

/**
 * @brief The model weights FIXINDEX pins, checked: 1-based, in declaration
 * order, the i-th the weight the i-th FIXWEIGHT value pins.
 *
 * The one reader of FIXINDEX (the trainers M_BLR_Train and M_KRR_Train,
 * MLIP's weight provisioning and BIAS composition check, tadah hpo's OPTIM
 * FIXWEIGHT mapping and startup checks). Empty when @p ctx has neither
 * FIXINDEX nor FIXWEIGHT.
 *
 * The values of FIXINDEX form one list, with the grammar of
 * parse_one_based_indices(): entries separated by commas or whitespace
 * ('FIXINDEX 13 14', 'FIXINDEX 13,14' and 'FIXINDEX 13, 14' are the same
 * list), each 'i', 'a-b' or 'a-b:step', with spaces allowed around '-' and
 * ':'. The order is kept.
 *
 * Refused with std::runtime_error, one message per error naming the text:
 * a lone FIXINDEX or FIXWEIGHT; an empty entry, a token that is not an
 * index, a descending range and a step of 0 ("FIXINDEX: index '14-13' is a
 * descending range (...)"); index 0, and, when @p n_weights is given, an
 * index above it ("FIXINDEX: index 9 (in '2-9') is outside 1-4 (the model
 * has 4 weights)"); a weight named twice among the first n + 1 indices, n
 * the number of FIXWEIGHT values ("FIXINDEX 13,13,14 names coefficient
 * 13 twice; ...", the first weight named again), since FIXWEIGHT pairs by
 * position: so every list with a repeat that names no more distinct
 * weights than FIXWEIGHT has values ('13,13,14' with two values, one per
 * distinct weight, as with three); otherwise a number of indices other
 * than the number of FIXWEIGHT values ("FIXINDEX 1,2,3,1 has 4 indices but
 * FIXWEIGHT has 2 values; ..."). The indices are counted per entry, and
 * no more than n + 1 of them are expanded, so '1-4000000000' is refused at
 * once ("FIXINDEX 1-4000000000 has 4000000000 indices but FIXWEIGHT has 2
 * values; ...").
 *
 * @param ctx       the configuration (FIXINDEX and FIXWEIGHT)
 * @param n_weights the number of model weights, when known; std::nullopt
 *                  checks every rule but the upper bound
 */
std::vector<size_t> parse_fixindex(const Context& ctx,
                                   std::optional<size_t> n_weights);


/**
 * @brief Move specified columns in place to the right end of the matrix.
 *
 * This function rearranges the columns of the matrix such that the columns
 * specified by indices are moved to the right, and returns a mapping
 * of the original index to the new index.
 *
 * @param matrix The matrix whose columns are to be rearranged.
 * @param indices A vector of column indices to move to the right.
 * @return A vector mapping original column indices to their new positions.
 */
template <typename T>
std::vector<size_t> move_columns_in_place(Matrix_Base<T>& matrix, const std::vector<size_t>& indices) {
  size_t N = matrix.cols();
  size_t num_indices = indices.size();
  size_t boundary = N - num_indices;

  std::vector<size_t> column_map(N);
  for (size_t i = 0; i < N; ++i) {
    column_map[i] = i;
  }

  size_t insertPos = 0;

  for (size_t i = 0; i < N; ++i) {
    if (insertPos < boundary && 
      std::find(indices.begin(), indices.end(), i) == indices.end()) {
      matrix.swap_columns(insertPos, i);

      std::swap(column_map[insertPos], column_map[i]);

      insertPos++;
    }
  }

  return column_map;
}

/**
 * @brief Reverses the column arrangement of the matrix using a given column map.
 *
 * The function restores the original column order of the matrix based on a mapping
 * that was used to rearrange the columns initially.
 * 
 * @param matrix The matrix that needs its columns restored to the original order.
 * @param column_map A vector mapping current column indices to their original positions.
 */
template <typename T>
void reverse_columns(Matrix_Base<T> &matrix, const std::vector<size_t>& column_map) {
    std::vector<size_t> temp_map = column_map;
    for (size_t i = 0; i < temp_map.size(); ++i) {
        while (temp_map[i] != i) {
            size_t original_pos = temp_map[i];
            matrix.swap_columns(i, original_pos);
            std::swap(temp_map[i], temp_map[original_pos]);
        }
    }
}

/**
 * @brief Reverses a vector arrangement using a given map.
 *
 * @param vec The vector to restore.
 * @param original_map A mapping of the rearranged indices to original positions.
 */
void reverse_vector(t_type& vec, const std::vector<size_t>& original_map);

/**
 * @typedef histogram_bin
 * @brief Type alias for a histogram bin containing the bin center coordinates and the count.
 *
 * Each histogram bin is represented as a tuple containing:
 * - `double x_center`: The center coordinate of the bin along the x-axis.
 * - `double y_center`: The center coordinate of the bin along the y-axis.
 * - `size_t count`: The number of data points that fall into the bin.
 */
typedef std::tuple<double, double, size_t> histogram_bin;

/**
 * @brief Generates a 2D histogram from input data.
 *
 * This function takes a 2D array of doubles representing (x, y) data points and generates
 * a 2D histogram. The histogram is returned as a vector of tuples, where each tuple contains
 * the x and y coordinates of the bin center and the count of data points within that bin.
 * The minimum and maximum values along each axis are calculated from the input data.
 *
 * @param data_2d A vector containing the (x, y) data points.
 * @param x_bins The number of bins along the x-axis.
 * @param y_bins The number of bins along the y-axis.
 * @return A vector of histogram bins, each containing the bin center coordinates and the count.
 */
std::vector<histogram_bin> generate_2d_histogram(
    const std::vector<std::pair<double,double>>& data_2d,
    int x_bins,
    int y_bins);


/**
 * @brief Base case for appendToStream: handles the scenario when no arguments remain.
 *
 * This function is the termination point for the recursive template overloads. 
 * When the parameter pack is empty, it simply returns.
 *
 * @param oss Reference to the output string stream to which data is appended.
 */
inline void appendToStream([[maybe_unused]] std::ostringstream &oss) {}

/**
 * @brief Appends a variable number of arguments into an std::ostringstream.
 *
 * This function processes one argument at a time and then recurses, 
 * eventually calling the base case when no arguments remain. It uses 
 * perfect forwarding to preserve value categories (lvalue or rvalue).
 *
 * @tparam T    The type of the first argument.
 * @tparam Rest Parameter pack representing the remaining argument types.
 *
 * @param oss   Reference to the output string stream to which data is appended.
 * @param first The first argument that is appended to the stream.
 * @param rest  The remaining arguments to be appended.
 *
 * @note
 * By using std::forward, we avoid unnecessary copies or moves, which 
 * can be particularly important for performance-critical code.
 *
 * ### Example
 * \code{.cpp}
 * std::ostringstream oss;
 * appendToStream(oss, "The number is: ", 42, '\n');
 * std::cout << oss.str(); // Outputs: "The number is: 42\n"
 * \endcode
 */
template <typename T, typename... Rest>
inline void appendToStream(std::ostringstream &oss, T&& first, Rest&&... rest) {
    oss << std::forward<T>(first);
    appendToStream(oss, std::forward<Rest>(rest)...);
}

/**
 * @brief Removes the first known word from the beginning of a string if present.
 *
 * This function compares the beginning of the input string to the known word
 * and, if it matches, removes that word and any immediately trailing spacing.
 * If the known word is absent or does not appear at the start, no removal is performed.
 *
 * @param input The string from which the known word is removed.
 * @param knownWord The known word that is expected at the start of the string.
 * @return A new string with the known word (and subsequent space) removed at the start.
 *
 * Example:
 * @code
 * #include <iostream>
 * int main() {
 *   std::string line = "Hello World!";
 *   std::string result = removeFirstWord(line, "Hello");
 *   // result now contains "World!"
 *   std::cout << result << std::endl;
 *   return 0;
 * }
 * @endcode
 */
std::string removeFirstWord(const std::string &input, const std::string &knownWord);

/**
 * @brief Concatenates a vector of strings into a single string with a specified delimiter.
 *
 * This function takes a vector of strings and joins them into a single string,
 * with each element separated by the specified delimiter. The default delimiter
 * is a space character.
 *
 * @param vec The vector of strings to concatenate.
 * @param delim The delimiter to use between elements (default is " ").
 * @return A single string containing all elements of the vector, separated by the delimiter.
 */
std::string concat(const std::vector<std::string>& vec, const std::string& delim = " ");

/**
 * @brief
 *   Wraps text at a specified column width, optionally preserving word boundaries and
 *   prepending a padding string only to lines that result from "breaking" (i.e., wrapped segments),
 *   thereby leaving the first line of each original input line unpadded.
 *
 * Behavior overview:
 *   - Existing newlines in the input are respected. Each original line (split by '\n') is processed independently.
 *   - If preserveWords = false, each line is split into consecutive chunks of size maxWidth.
 *   - If preserveWords = true, each line is split at word boundaries to avoid breaking words. 
 *     If a word is longer than maxWidth, it is placed on its own line.
 *   - For each original line (prior to wrapping), the first wrapped segment is unpadded; subsequent segments are padded.
 *
 * @param text
 *   The full text to be wrapped. May contain existing newlines.
 * @param maxWidth
 *   Maximum number of characters allowed per line. This must be a positive integer greater than zero.
 * @param preserveWords
 *   If true, the function attempts to keep words intact on each line by splitting on whitespace.
 *   If false, lines are cut at the exact maxWidth boundary (unless shorter).
 * @param linePadding
 *   A string prepended only to "broken" lines (second and subsequent lines in each wrapped group).
 *
 * @return
 *   A new string containing the wrapped text. Each line is terminated by a newline ('\n').
 *
 * @code
 * #include <iostream>
 *
 * int main() {
 *   std::string input =
 *     "This sample text demonstrates two wrapping techniques:\n"
 *     "  1) chunk wrapping.\n"
 *     "  2) word wrapping.\n"
 *     "A high degree of flexibility is provided, including preserving existing newlines.\n";
 *
 *   // Example: Wrap text to 50 characters wide, preserve words, and pad only broken lines with ">> "
 *   std::string wrappedText = wrapText(input, 50, true, ">> ");
 *
 *   std::cout << "Wrapped Text:\n" << wrappedText << std::endl;
 *
 *   return 0;
 * }
 * @endcode
 */
std::string wrapText(const std::string &text,
    std::size_t maxWidth, bool preserveWords,
    const std::string &linePadding);

} // namespace core
} // namespace tadah
#endif
