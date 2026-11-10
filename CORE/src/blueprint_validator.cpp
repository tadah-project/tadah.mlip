/**
 *  @file blueprint_validator.cpp
 *  @brief Implementation of tadah::core::BlueprintValidator
 *
 *  validate() splits a line into words at blanks, as CoreUserConfigParser
 *  splits a configuration line into values, and matches the words against
 *  the blueprint compiled to a tree: the set of word positions each part of
 *  the blueprint can end at is carried from part to part, so a line is
 *  decided in time linear in its number of words, whatever its length and
 *  whether it is valid (no std::regex backtracking, which took time
 *  exponential in the number of values to refuse a line and overflows the
 *  stack on long input). A comma or a '+' between two values is not a
 *  separator (a STRING word keeps them; an index list, INDEX_PATTERN, takes
 *  commas between its entries, which its readers split at). getPattern()
 *  and getCaptures() keep the blueprint's anchored regex.
 */

#include <tadah/core/blueprint_validator.h>

#include <regex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <toml.hpp>

namespace tadah {
namespace core {

// ──────────────────────────────────────────────────────────────
// Primitive fragments
// ──────────────────────────────────────────────────────────────
static const std::unordered_map<std::string,std::string> g_primitiveMap = {
  {"INT",    "\\s*[-+]?\\d+\\s*"},
  {"DOUBLE", "\\s*[-+]?\\d+(?:\\.\\d+)?(?:[eE][-+]?\\d+)?\\s*"},
  {"STRING", "\\s*[^\\s]*[A-Za-z*][^\\s]*\\s*"},
  {"UINT",   "\\s*(?:0|[1-9]\\d*)\\s*"},
  {"BOOL",   "\\s*(?:true|false)\\s*"},
  // An index list: entries i, a-b or a-b:step, separated by commas (blanks
  // around a comma, '-' or ':' allowed); a list key's values ({...}) are
  // separated by blanks, so "13,14", "13, 14" and "13 14" are one list, as
  // parse_one_based_indices and parse_fixindex read them.
  {"INDEX_PATTERN",
   "\\s*[+]?\\d+(?:\\s*-\\s*[-+]?\\d+(?:\\s*:\\s*[+]?\\d+)?){0,1}"
   "(?:\\s*,\\s*[+]?\\d+(?:\\s*-\\s*[-+]?\\d+(?:\\s*:\\s*[+]?\\d+)?){0,1})*"
   "\\s*" }
};

// ──────────────────────────────────────────────────────────────
// Small utilities
// ──────────────────────────────────────────────────────────────
static std::string trim(const std::string& s)
{
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b==std::string::npos) return "";
  const auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b,e-b+1);
}

static std::vector<std::string> splitBlueprint(const std::string& bp)
{
  std::vector<std::string> out;
  std::string tok;
  bool inGroup=false; char end=0;

  for(char c:bp){
    if(!inGroup&&(c=='['||c=='{')){
      if(!tok.empty()){out.push_back(trim(tok));tok.clear();}
      inGroup=true; end=(c=='[')?']':'}'; tok.push_back(c);
    }
    else if(inGroup){
      tok.push_back(c);
      if(c==end){ inGroup=false; out.push_back(trim(tok)); tok.clear(); }
    }
    else{
      if(std::isspace((unsigned char)c)){ if(!tok.empty()){out.push_back(trim(tok));tok.clear();} }
      else tok.push_back(c);
    }
  }
  if(!tok.empty()) out.push_back(trim(tok));
  return out;
}

static std::vector<std::string>
reorderAlternatives(std::vector<std::string> alts)
{
  auto rank=[](const std::string&s){ if(s=="UINT")return 1; if(s=="INT")return 2;
                                     if(s=="DOUBLE")return 3;
                                     return 9999; };
  std::sort(alts.begin(),alts.end(),
            [&](const std::string&a,const std::string&b){return rank(a)<rank(b);});
  return alts;
}

// ──────────────────────────────────────────────────────────────
// Forward declarations
// ──────────────────────────────────────────────────────────────
static std::string buildTokenRegex(const std::string&);
static std::string buildRegexFromBlueprint(const std::string&,bool);

static bool isBlank(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

// ──────────────────────────────────────────────────────────────
// The word matcher of validate()
// ──────────────────────────────────────────────────────────────
namespace {

using Words = std::vector<std::string_view>;
// Word positions, in increasing order, each once: position i is reached when
// the words before word i are matched (i = 0 .. number of words).
using Reach = std::vector<std::size_t>;

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

// [-+]?\d+ from p; advances p.
static bool digits(const char*& p, const char* e, bool sign) {
  if (sign && p < e && (*p == '+' || *p == '-')) ++p;
  const char* b = p;
  while (p < e && isDigit(*p)) ++p;
  return p > b;
}

// The primitives of one word, as their regex fragments define them.
static bool isInt(std::string_view w) {
  const char *p = w.data(), *e = p + w.size();
  return digits(p, e, true) && p == e;
}
static bool isUint(std::string_view w) {        // 0|[1-9]\d*
  if (w.empty() || !isDigit(w[0]) || (w[0] == '0' && w.size() > 1)) return false;
  for (char c : w) if (!isDigit(c)) return false;
  return true;
}
static bool isDouble(std::string_view w) {      // [-+]?\d+(\.\d+)?([eE][-+]?\d+)?
  const char *p = w.data(), *e = p + w.size();
  if (!digits(p, e, true)) return false;
  if (p < e && *p == '.') { ++p; if (!digits(p, e, false)) return false; }
  if (p < e && (*p == 'e' || *p == 'E')) { ++p; if (!digits(p, e, true)) return false; }
  return p == e;
}
static bool isString(std::string_view w) {      // a word with a letter or '*'
  for (char c : w)
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '*') return true;
  return false;
}
static bool isBool(std::string_view w) { return w == "true" || w == "false"; }

/* The word positions after an index list that starts at word `first`:
 * entries i, a-b or a-b:step ([+]digits, '-', [+-]digits, ':', [+]digits),
 * separated by commas; blanks around '-', ':' and ',' allowed, so an entry
 * or a comma may span words. The list ends at the end of a word that ends
 * an entry. */
static void indexListEnds(const Words& words, std::size_t first,
                          std::vector<std::size_t>& ends) {
  enum State { E0, E0s, N1, N1b, R, Rs, N2, N2b, T, Ts, N3, N3b, C, Dead };
  auto step = [](State s, char c) -> State {
    const bool d = isDigit(c), b = c == ' ';
    switch (s) {
      case E0:  return c == '+' ? E0s : d ? N1 : Dead;
      case E0s: return d ? N1 : Dead;
      case N1:  return d ? N1 : b ? N1b : c == '-' ? R : c == ',' ? C : Dead;
      case N1b: return b ? N1b : c == '-' ? R : c == ',' ? C : Dead;
      case R:   return b ? R : (c == '-' || c == '+') ? Rs : d ? N2 : Dead;
      case Rs:  return d ? N2 : Dead;
      case N2:  return d ? N2 : b ? N2b : c == ':' ? T : c == ',' ? C : Dead;
      case N2b: return b ? N2b : c == ':' ? T : c == ',' ? C : Dead;
      case T:   return b ? T : c == '+' ? Ts : d ? N3 : Dead;
      case Ts:  return d ? N3 : Dead;
      case N3:  return d ? N3 : b ? N3b : c == ',' ? C : Dead;
      case N3b: return b ? N3b : c == ',' ? C : Dead;
      case C:   return b ? C : c == '+' ? E0s : d ? N1 : Dead;
      default:  return Dead;
    }
  };
  State s = E0;
  for (std::size_t j = first; j < words.size(); ++j) {
    if (j > first) s = step(s, ' ');
    for (char c : words[j]) {
      s = step(s, c);
      if (s == Dead) return;
    }
    if (s == N1 || s == N2 || s == N3) ends.push_back(j + 1);
  }
}

enum class Prim { Int, Uint, Double, String, Bool, IndexList };

struct Node {
  enum class Kind { Primitive, Word, Sequence, Group, Alternatives };
  Kind kind = Kind::Sequence;
  Prim prim = Prim::Int;
  std::regex word;              // Word: a blueprint word that names no type
  std::vector<Node> children;   // Sequence: its parts; Group: one Sequence;
                                // Alternatives: one Sequence each
  bool optional = false;        // Alternatives of the line ([...] there)
};

static Node parseSequence(const std::string& bp, bool line);

/* One part of a blueprint: {..} a group, repeated once or more; [..]
 * alternatives separated by '|' or " or ", optional on the line itself
 * (`line`) and one of them inside a group; a type name; another word, a
 * regex fragment one word must match. */
static Node parsePart(const std::string& tok, bool line) {
  Node n;
  if (tok.size() >= 2 && tok.front() == '{' && tok.back() == '}') {
    n.kind = Node::Kind::Group;
    n.children.push_back(parseSequence(tok.substr(1, tok.size() - 2), false));
    return n;
  }
  if (tok.size() >= 2 && tok.front() == '[' && tok.back() == ']') {
    n.kind = Node::Kind::Alternatives;
    n.optional = line;
    std::string inner = tok.substr(1, tok.size() - 2);
    for (std::size_t pos = 0; (pos = inner.find(" or ", pos)) != std::string::npos;)
      { inner.replace(pos, 4, "|"); ++pos; }
    std::size_t start = 0;
    while (true) {
      const auto bar = inner.find('|', start);
      n.children.push_back(parseSequence(
          inner.substr(start, bar == std::string::npos ? std::string::npos
                                                       : bar - start),
          false));
      if (bar == std::string::npos) break;
      start = bar + 1;
    }
    return n;
  }
  static const std::unordered_map<std::string, Prim> prims = {
      {"INT", Prim::Int}, {"UINT", Prim::Uint}, {"DOUBLE", Prim::Double},
      {"STRING", Prim::String}, {"BOOL", Prim::Bool},
      {"INDEX_PATTERN", Prim::IndexList}};
  const auto it = prims.find(tok);
  if (it != prims.end()) {
    n.kind = Node::Kind::Primitive;
    n.prim = it->second;
  } else {
    n.kind = Node::Kind::Word;
    n.word = std::regex(tok, std::regex::ECMAScript | std::regex::nosubs);
  }
  return n;
}

static Node parseSequence(const std::string& bp, bool line) {
  Node n;
  n.kind = Node::Kind::Sequence;
  for (const auto& tok : splitBlueprint(trim(bp)))
    n.children.push_back(parsePart(tok, line));
  return n;
}

static Reach sortedUnique(Reach r) {
  std::sort(r.begin(), r.end());
  r.erase(std::unique(r.begin(), r.end()), r.end());
  return r;
}

/* The positions where `n` can end when it starts at a position of `from`.
 * The work is proportional to the positions reached (a group carries each
 * position on once), so a line is decided in time linear in its words. */
static Reach match(const Node& n, const Words& words, const Reach& from) {
  const std::size_t size = words.size();
  Reach to;
  switch (n.kind) {
    case Node::Kind::Primitive: {
      std::vector<std::size_t> ends;
      for (std::size_t i : from) {
        if (i >= size) continue;
        const std::string_view w = words[i];
        bool ok = false;
        switch (n.prim) {
          case Prim::Int:    ok = isInt(w); break;
          case Prim::Uint:   ok = isUint(w); break;
          case Prim::Double: ok = isDouble(w); break;
          case Prim::String: ok = isString(w); break;
          case Prim::Bool:   ok = isBool(w); break;
          case Prim::IndexList: indexListEnds(words, i, ends); break;
        }
        if (ok) to.push_back(i + 1);
      }
      if (n.prim == Prim::IndexList) to = sortedUnique(std::move(ends));
      break;
    }
    case Node::Kind::Word:
      for (std::size_t i : from)
        if (i < size &&
            std::regex_match(words[i].begin(), words[i].end(), n.word))
          to.push_back(i + 1);
      break;
    case Node::Kind::Sequence:
      to = from;
      for (const Node& part : n.children) {
        if (to.empty()) break;
        to = match(part, words, to);
      }
      break;
    case Node::Kind::Group: {
      // Once or more: every position reached is carried on from once.
      std::unordered_set<std::size_t> seen;
      Reach next = match(n.children.front(), words, from);
      while (!next.empty()) {
        Reach fresh;
        for (std::size_t i : next)
          if (seen.insert(i).second) fresh.push_back(i);
        to.insert(to.end(), fresh.begin(), fresh.end());
        next = match(n.children.front(), words, fresh);
      }
      to = sortedUnique(std::move(to));
      break;
    }
    case Node::Kind::Alternatives:
      if (n.optional) to = from;
      for (const Node& alt : n.children) {
        const Reach r = match(alt, words, from);
        to.insert(to.end(), r.begin(), r.end());
      }
      to = sortedUnique(std::move(to));
      break;
  }
  return to;
}

static bool matchesLine(const Node& bp, const Words& words) {
  if (bp.children.empty()) return false;  // an empty blueprint takes nothing
  const Reach end = match(bp, words, Reach{0});
  return std::binary_search(end.begin(), end.end(), words.size());
}

}  // namespace

// ──────────────────────────────────────────────────────────────
// Token-level regex helpers
// ──────────────────────────────────────────────────────────────
static std::string buildTokenRegex(const std::string& tok)
{
  if (tok.empty()) return "";

  if (tok.front()=='{' && tok.back()=='}') {
    const std::string inner = trim(tok.substr(1,tok.size()-2));
    const auto subs = splitBlueprint(inner);

    // The elements and the repetitions are separated by a blank, as the
    // parser splits a line (the primitives' own \\s* take more blanks).
    std::string block; bool first=true;
    for(const auto& s:subs){
      if(!first) block += "\\s";
      block += buildTokenRegex(s);
      first=false;
    }
    return "(?:"+block+")(?:\\s"+block+")*";
  }

  if (tok.front()=='[' && tok.back()==']') {
    std::string inner = trim(tok.substr(1,tok.size()-2));
    for(std::size_t pos=0;(pos=inner.find(" or ",pos))!=std::string::npos;)
      { inner.replace(pos,4,"|"); ++pos; }

    std::vector<std::string> alts;
    std::size_t start=0;
    while(true){
      auto bar=inner.find('|',start);
      if(bar==std::string::npos){ alts.push_back(trim(inner.substr(start))); break; }
      alts.push_back(trim(inner.substr(start,bar-start)));
      start=bar+1;
    }
    if(alts.size()>1) alts=reorderAlternatives(alts);

    std::string re="(?:";
    for(std::size_t i=0;i<alts.size();++i){
      auto it=g_primitiveMap.find(alts[i]);
      re+=(it!=g_primitiveMap.end())?it->second:alts[i];
      if(i+1<alts.size()) re+="|";
    }
    re+=")";
    return re;
  }

  auto it=g_primitiveMap.find(tok);
  return (it!=g_primitiveMap.end()) ? it->second : tok;
}

// blueprint-level
static std::string buildRegexFromBlueprint(const std::string& bp,bool cap)
{
  const auto toks = splitBlueprint(bp);
  if(toks.empty()) return "^(?!).*$";

  auto open=[&](bool want){return std::string(want?"(":"(?:");};
  const std::string close=")";

  std::string pat="^";
  bool first=true;
  for(const auto& t:toks){
    bool opt=(!t.empty()&&t.front()=='['&&t.back()==']');
    if(first){
      first=false;
      pat+= opt ? open(cap)+buildTokenRegex(t)+")?"
                : open(cap)+buildTokenRegex(t)+close;
    }else{
      if(opt)
        pat+="(?:\\s+"+open(cap)+buildTokenRegex(t)+close+")?";
      else
        pat+="\\s+"  +open(cap)+buildTokenRegex(t)+close;
    }
  }
  pat+="\\s*$";
  return pat;
}

// ──────────────────────────────────────────────────────────────
// pImpl
// ──────────────────────────────────────────────────────────────
class BlueprintValidator::Impl {
public:
  explicit Impl(const std::vector<std::string>& blueprints) {
    lines.reserve(blueprints.size());
    for (const auto& bp : blueprints) lines.push_back(parseSequence(bp, true));
  }
  std::vector<Node> lines;  // one tree per blueprint
};

// ──────────────────────────────────────────────────────────────
// Constructors
// ──────────────────────────────────────────────────────────────
BlueprintValidator::BlueprintValidator(const std::string& bp)
{
  m_patterns = buildRegexFromBlueprint(bp,true);
  m_impl = new Impl({bp});
}

BlueprintValidator::BlueprintValidator(const std::vector<std::string>& bps)
{
  bool first=true;
  for(auto& bp:bps){
    const auto cap=buildRegexFromBlueprint(bp,true);
    if(!first) m_patterns+='\n';
    m_patterns+=cap; first=false;
  }
  m_impl = new Impl(bps);
}

// ──────────────────────────────────────────────────────────────
// validate()
// ──────────────────────────────────────────────────────────────
bool BlueprintValidator::validate(const std::string& text) const
{
  if(!m_impl) return false;
  // The words of the line, split at blanks as the parser splits it.
  Words words;
  for (std::size_t i = 0; i < text.size();) {
    while (i < text.size() && isBlank(text[i])) ++i;
    const std::size_t b = i;
    while (i < text.size() && !isBlank(text[i])) ++i;
    if (i > b) words.emplace_back(text.data() + b, i - b);
  }
  for (const Node& line : m_impl->lines)
    if (matchesLine(line, words)) return true;
  return false;
}

bool BlueprintValidator::validate(const std::vector<std::string>& toks) const
{
  std::string concat;
  for(std::size_t i=0;i<toks.size();++i){
    concat+=toks[i];
    if(i+1<toks.size()) concat+=' ';
  }
  return validate(concat);
}

// ──────────────────────────────────────────────────────────────
// getCaptures() – unchanged logic
// ──────────────────────────────────────────────────────────────
std::vector<std::string>
BlueprintValidator::getCaptures(const std::string& text) const
{
  std::vector<std::string> out;
  if(m_patterns.empty()) return out;

  int best=-1; std::smatch bestMatch;
  std::istringstream iss(m_patterns); std::string line;

  while(std::getline(iss,line)){
    if(line.empty()) continue;
    std::regex cre(line,std::regex::ECMAScript);
    std::smatch m;
    if(std::regex_match(text,m,cre)){
      for(std::size_t i=1;i<m.size();++i) out.push_back(m[i].str());
      return out;
    }else if(std::regex_search(text,m,cre)){
      int len=(int)m.length(0); if(len>best){best=len; bestMatch=m;}
    }
  }
  if(out.empty()&&best>0)
    for(std::size_t i=1;i<bestMatch.size();++i) out.push_back(bestMatch[i].str());
  return out;
}

// convenience helpers / destructor
bool BlueprintValidator::validateOnce(const std::string& bp,const std::string& txt){
  BlueprintValidator v(bp); return v.validate(txt);
}
bool BlueprintValidator::validateOnce(const std::vector<std::string>& bps,
                                      const std::string& txt){
  BlueprintValidator v(bps); return v.validate(txt);
}
std::string BlueprintValidator::getPattern() const { return m_patterns; }
BlueprintValidator::~BlueprintValidator(){ delete m_impl; m_impl=nullptr; }

} // namespace core
} // namespace tadah
