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
`group` and `rank` fields); cross-key rules render as constraint sentences
in the description column, so documentation cannot drift from validation.

The HPO checkout is found by source_locator.py: $TADAH_HPO_SOURCE_DIR, then
the HPO checkout next to this repository, then $TADAH_PATH/HPO, then (on
GitLab CI / Read the Docs only) a shallow clone of the branch being built.
There is no fallback branch: a missing registry fails the docs build with
the list of locations tried.
"""

from __future__ import annotations

import os
import sys

try:
    import tomllib

    def _load(path):
        with open(path, "rb") as fh:
            return tomllib.load(fh)
except ImportError:  # pragma: no cover - pre-3.11 fallback
    import toml

    def _load(path):
        return toml.load(path)

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


#: Presence-tested or run-time-derived defaults, described rather than
#: numeric (the registry has no static value for these kinds).
AUTO_DEFAULT_TEXT = {
    ("CHAIN_LENGTH", "serial"): "auto (budget)",
    ("NT", "serial"): "auto (budget)",
    ("T_INIT", "serial"): "auto (calibrated)",
    ("T_HOT", "pt"): "auto (calibrated)",
    ("T_HOT", "pa"): "auto (calibrated)",
    ("T_MIN", "serial"): "auto (T0 * 1e-6)",
    ("T_MIN", "pa"): "auto (T0 * 1e-6)",
    ("T_COLD", "pt"): "auto (T_HOT / LADDER_SPAN)",
    ("RESET_TO_BEST", "serial"): "``OFF`` (``ON`` with ``CORANA``)",
    ("RESET_TO_BEST", "pt"): "``OFF``",
    ("REPLICAS", "pt"): "max(THREADS, 2)",
}


def default_for(key: dict, kind: str) -> str:
    special = AUTO_DEFAULT_TEXT.get((key["name"], kind))
    if special is not None:
        return special
    raw = key.get(f"default_{kind}", key.get("default"))
    if raw is None:
        return "``AUTO``" if key.get("accepts_auto") is not None else "auto"
    if key["type"] == "bool":
        return "``ON``" if str(raw).strip() in ("1", "1.0", "true", "True") else "``OFF``"
    # 1.5.0: these keys carry their AUTO/OFF sentinel AS the numeric default,
    # so printing the raw value advertises a number the parser then rejects
    # (INIT_JITTER_LADDER would read "default -1"). Print the token the user
    # actually types.
    for field, token in (("accepts_auto", "``AUTO``"), ("accepts_off", "``OFF``")):
        sentinel = key.get(field)
        if sentinel is not None and float(raw) == float(sentinel):
            return token
    if key["type"] == "float":
        return _num(raw)
    return str(raw)


def _token_suffix(key: dict) -> str:
    """The 1.5.0 AUTO/OFF tokens a numeric key also accepts."""
    toks = [t for t, f in (("``AUTO``", "accepts_auto"), ("``OFF``", "accepts_off"))
            if key.get(f) is not None]
    return (" | " + " | ".join(toks)) if toks else ""


def range_for(key: dict) -> str:
    if key["type"] == "enum":
        return " | ".join(f"``{v}``" for v in key["values"])
    if key["type"] == "bool":
        return "``ON`` | ``OFF``"
    if key.get("custom_range"):
        # No table range: the check is hand-written in anneal::Knobs::parse.
        base = "integer >= 1" if key["type"] == "count" else "float"
        return base + _token_suffix(key)
    has_min, has_max = "min" in key, "max" in key
    suffix = _token_suffix(key)
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
    out = []
    for rule in rules:
        if rule.get("key") != name:
            continue
        ref = rule.get("flag") or rule.get("other") or rule.get("enum_key")
        if ref is not None and ref not in kind_keys:
            continue   # rule references a key of a different family member
        kind = rule["type"]
        if kind == "requires_flag":
            out.append(f"Requires ``{rule['flag']} ON``.")
        elif kind == "conflicts_flag":
            out.append(f"Invalid with ``{rule['flag']} ON``.")
        elif kind == "unused_when_set":
            out.append(f"Unused when ``{rule['other']}`` is set.")
        elif kind == "unused_when_flag_off":
            out.append(f"Unused when ``{rule['flag']}`` is ``OFF``.")
        elif kind == "requires_enum":
            out.append(
                f"Only with ``{rule['enum_key']} {rule['value']}``.")
    return out


def render(kind: str, data: dict) -> str:
    keys = [k for k in data.get("key", []) if kind in k["kinds"]]
    kind_keys = {k["name"] for k in keys}
    rules = data.get("rule", [])
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
            "   :widths: 22 12 16 50",
            "",
            "   * - Key",
            "     - Default",
            "     - Values",
            "     - Description",
        ]
        for k in rows:
            doc = k["doc"]
            extra = constraint_sentences(k["name"], rules, kind_keys)
            if extra:
                doc = doc.rstrip(".") + ". " + " ".join(extra)
            lines += [
                f"   * - ``{k['name']}``",
                f"     - {default_for(k, kind)}",
                f"     - {range_for(k)}",
                f"     - {doc}",
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
