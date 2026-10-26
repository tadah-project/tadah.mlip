#!/usr/bin/env python3
"""Generate the ANNEAL-family key tables from HPO's key registry.

Single source of truth: HPO/blueprint/anneal_keys.toml (the same file that
generates the C++ parser tables and the CLI capability text, guarded by the
ctest hpo_anneal_registry_sync drift gate). This script renders it into
three includable .rst fragments:

    config/anneal_keys_serial.rst   (TADAH:ANNEAL)
    config/anneal_keys_pt.rst       (TADAH:ANNEAL_PT)
    config/anneal_keys_pa.rst       (TADAH:ANNEAL_PA)

Keys are grouped by function and ordered by importance (the registry's
`group` and `rank` fields). Each key has a reference cell with separate
default, value and description paragraphs. Cross-key rules render as
sentences after the description, each the refusal it is (the annealers
refuse a key its rule does not allow), so documentation cannot drift from
validation.

The HPO checkout is found by source_locator.py: $TADAH_HPO_SOURCE_DIR, then
the HPO checkout next to this repository, then $TADAH_PATH/HPO, then (on
GitLab CI / Read the Docs only) a shallow clone of the branch being built.
There is no fallback branch: a missing registry fails the docs build with
the list of locations tried.
"""

from __future__ import annotations

import os
import re
import sys

# The registry is read with tomllib, in the standard library from Python
# 3.11; the documentation build needs Python 3.11 or later (CONTRIBUTING.md,
# "Building the documentation"), and an older interpreter stops it here.
try:
    import tomllib
except ImportError:
    sys.exit(
        "read_anneal_keys.py needs Python 3.11 or later: it reads HPO's "
        "blueprint/anneal_keys.toml with tomllib, which Python "
        f"{sys.version_info.major}.{sys.version_info.minor} does not have "
        f"({sys.executable}); build the documentation with Python 3.11 or "
        "later")


def _load(path):
    with open(path, "rb") as fh:
        return tomllib.load(fh)

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import source_locator  # noqa: E402

ANNEAL_KEYS_REL = os.path.join("blueprint", "anneal_keys.toml")
KINDS = {
    "serial": ("anneal_keys_serial.rst", "TADAH:ANNEAL"),
    "pt": ("anneal_keys_pt.rst", "TADAH:ANNEAL_PT"),
    "pa": ("anneal_keys_pa.rst", "TADAH:ANNEAL_PA"),
}
GROUP_ORDER = ["temperature", "ladder", "swaps", "steps", "init", "stopping",
               "budget"]
GROUP_TITLES = {
    "temperature": "Temperature",
    "ladder": "Ladder",
    "swaps": "Exchange and diagnostics cadence",
    "steps": "Proposal steps",
    "init": "Replica initialisation",
    "stopping": "Stopping",
    "budget": "Budget",
}


def locate_toml() -> str:
    return source_locator.locate_file("HPO", ANNEAL_KEYS_REL)


#: A run of upper-case names and tokens in a default in words (T_HOT,
#: CORANA ON): each run is one literal.
_UPPER_RUN = re.compile(r"\b[A-Z][A-Z0-9_]+(?: [A-Z][A-Z0-9_]+)*\b")


def default_for(key: dict, kind: str) -> str:
    """The default of *key* under *kind*: the registry's default in words
    (default_doc_<kind>, else default_doc), with its upper-case names and
    tokens as literals, or its value. A kind with neither has no default
    to show, which stops the generator."""
    words = key.get(f"default_doc_{kind}", key.get("default_doc"))
    if words is not None:
        return _UPPER_RUN.sub(lambda m: f"``{m.group(0)}``", words)
    raw = key.get(f"default_{kind}", key.get("default"))
    if raw is None:
        raise ValueError(f"anneal_keys.toml: key {key['name']} has no "
                         f"default and no default_doc for {kind}")
    if key["type"] == "bool":
        return "``ON``" if str(raw).strip() in ("1", "1.0", "true", "True") else "``OFF``"
    # 1.5.0: these keys carry their AUTO/OFF sentinel AS the numeric default,
    # so printing the raw value advertises a number the parser then rejects
    # (INIT_JITTER_LADDER would read "default -1"). Print the token the user
    # actually types.
    for field, token in (("accepts_auto", "``AUTO``"), ("accepts_off", "``OFF``")):
        sentinel = key.get(field)
        if field == "accepts_auto" and not _takes_auto(key, kind):
            continue
        if sentinel is not None and float(raw) == float(sentinel):
            return token
    if key["type"] == "float":
        return _num(raw)
    return str(raw)


def _takes_auto(key: dict, kind: str) -> bool:
    """Whether the key accepts AUTO in `kind`: its auto_kinds, or every kind
    of the key when the field is absent."""
    return (key.get("accepts_auto") is not None and
            kind in key.get("auto_kinds", key["kinds"]))


def _token_suffix(key: dict, kind: str) -> str:
    """The 1.5.0 AUTO/OFF tokens a numeric key also accepts in `kind`."""
    toks = []
    if _takes_auto(key, kind):
        toks.append("``AUTO``")
    if key.get("accepts_off") is not None:
        toks.append("``OFF``")
    return (" | " + " | ".join(toks)) if toks else ""


def range_for(key: dict, kind: str) -> str:
    if key["type"] == "enum":
        return " | ".join(f"``{v}``" for v in key["values"])
    if key["type"] == "bool":
        return "``ON`` | ``OFF``"
    if key.get("custom_range"):
        # No table range: the check is hand-written in anneal::Knobs::parse,
        # and the registry says the range in words (range_doc).
        if key.get("range_doc"):
            return key["range_doc"]
        base = "integer >= 1" if key["type"] == "count" else "float"
        return base + _token_suffix(key, kind)
    has_min, has_max = "min" in key, "max" in key
    suffix = _token_suffix(key, kind)
    if not has_min and not has_max:
        return ("integer >= 0" if key["type"] == "count" else "float") + suffix
    lo = "(" if key.get("min_open") else "["
    hi = ")" if key.get("max_open") else "]"
    lo_v = _num(key["min"]) if has_min else "-inf"
    hi_v = _num(key["max"]) if has_max else "inf"
    if has_min and not has_max:
        op = ">" if key.get("min_open") else ">="
        return f"{op} {lo_v}" + suffix
    if has_max and not has_min:
        op = "<" if key.get("max_open") else "<="
        return f"{op} {hi_v}" + suffix
    return f"{lo}{lo_v}, {hi_v}{hi}" + suffix


def _num(v) -> str:
    f = float(v)
    return str(int(f)) if f == int(f) else str(f)


def constraint_sentences(name: str, rules: list, kind_keys: set) -> list[str]:
    """The rules of the key *name* as sentences, each the refusal it is: an
    annealer refuses a key that its rule does not allow (anneal_core.h),
    so "unused when T_INIT is set" is a refusal, not a key ignored. A rule
    that names a key of another annealer is left out."""
    out = []
    for rule in rules:
        if rule.get("key") != name:
            continue
        ref = rule.get("flag") or rule.get("other") or rule.get("enum_key")
        if ref is not None and ref not in kind_keys:
            continue   # rule references a key of a different family member
        kind = rule["type"]
        if kind in ("requires_flag", "unused_when_flag_off"):
            text = f"Refused unless ``{rule['flag']}`` is ``ON``."
        elif kind == "conflicts_flag":
            text = f"Refused when ``{rule['flag']}`` is ``ON``."
        elif kind == "unused_when_set":
            text = f"Refused when ``{rule['other']}`` is set."
        elif kind == "requires_enum":
            text = (f"Refused unless ``{rule['enum_key']}`` is "
                    f"``{rule['value']}``.")
        else:
            continue
        if text not in out:
            out.append(text)
    return out


def render(kind: str, data: dict) -> str:
    rules = data.get("rule", [])
    # A key the kind refuses (a kind_reject rule) is not one of its keys.
    refused = {r["key"] for r in rules
               if r.get("type") == "kind_reject" and r.get("kind") == kind}
    keys = [k for k in data.get("key", [])
            if kind in k["kinds"] and k["name"] not in refused]
    kind_keys = {k["name"] for k in keys}
    lines = [
        f".. Generated by config/read_anneal_keys.py from HPO's",
        f"   blueprint/anneal_keys.toml -- do not edit by hand.",
        "",
    ]
    for group in GROUP_ORDER:
        rows = sorted((k for k in keys if k["group"] == group),
                      key=lambda k: k["rank"])
        if not rows:
            continue
        lines += [
            f".. rubric:: {GROUP_TITLES[group]}",
            "",
            ".. list-table::",
            "   :header-rows: 1",
            "   :widths: 28 72",
            "",
            "   * - Key",
            "     - Reference",
        ]
        for k in rows:
            doc = k["doc"]
            if k.get("long_doc"):
                doc = doc.rstrip(".") + ". " + k["long_doc"]
            extra = constraint_sentences(k["name"], rules, kind_keys)
            if extra:
                doc = doc.rstrip(".") + ". " + " ".join(extra)
            lines += [
                f"   * - ``{k['name']}``",
                f"     - Default: {default_for(k, kind)}",
                "",
                f"       Values: {range_for(k, kind)}",
                "",
                f"       {doc}",
            ]
        lines.append("")
    return "\n".join(lines) + "\n"


def main() -> None:
    data = _load(locate_toml())
    for kind, (fname, _) in KINDS.items():
        out = os.path.join(HERE, fname)
        with open(out, "w") as fh:
            fh.write(render(kind, data))
        print(f"read_anneal_keys.py: wrote {out}")


if __name__ == "__main__":
    try:
        main()
    except source_locator.SourceNotFound as err:
        sys.exit(f"read_anneal_keys.py: {err}")
