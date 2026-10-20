#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import toml
import os
import textwrap
from copy import deepcopy

###############################################################################
# Data classes
###############################################################################

class BlueprintKey:
    def __init__(self):
        self.keyName = ""
        self.value_type = []
        self.value_format = []
        self.value_N = []
        self.description = ""
        self.longDescription = ""
        self.examples = []
        self.default_value = []
        self.cli_long_flag = ""
        self.cli_short_flag = ""
        self.cli_excludes = []
        self.cli_needs = []
        self.cli_required = False
        # CORE's blueprint field `retired` (reason, replacement and
        # conversion); empty for a key that is not retired.
        self.retired = {}

class BlueprintKeyGroup:
    def __init__(self):
        self.groupName = ""
        self.description = []
        self.blueprintKeys = []
        self.cli_required = False
        self.requiredMin = 1
        self.requiredMax = 1
        self.cli_excludes = []
        self.cli_needs = []

class BlueprintTask:
    def __init__(self):
        self.taskName = ""
        self.description = ""
        self.longDescription = ""
        self.examples = []
        self.blueprintKeys = []
        self.groups = []

class BlueprintTadah(BlueprintTask):
    def __init__(self):
        super().__init__()
        self.logo = ""
        self.version = ""
        self.website = ""

###############################################################################
# Inheritance expansion
###############################################################################

def expand_inheritance(table, root, parent_scope=""):
    if not isinstance(table, dict):
        return
    for k, v in list(table.items()):
        if isinstance(v, dict):
            new_scope = f"{parent_scope}.{k}" if parent_scope else k
            expand_inheritance(v, root, new_scope)

    if "INHERIT" in table:
        inherits = table["INHERIT"]
        if not isinstance(inherits, list):
            raise RuntimeError(f"'INHERIT' must be a list in {parent_scope}")
        for inh in inherits:
            parts = inh.split(".")
            src_node = deep_get(root, parts)
            if not isinstance(src_node, dict):
                raise RuntimeError(f"Inheritance source is not a table: {inh}")
            fromcopy = deepcopy(src_node)
            expand_inheritance(fromcopy, root, inh)
            merge_into(table, fromcopy)
        del table["INHERIT"]

def merge_into(dest, src):
    for k, v in src.items():
        if k not in dest:
            dest[k] = v

def deep_get(root, path_parts):
    cur = root
    for p in path_parts:
        if not isinstance(cur, dict) or p not in cur:
            raise RuntimeError(
                f"Invalid path: {'.'.join(path_parts)}. Missing '{p}'."
            )
        cur = cur[p]
    return cur

###############################################################################
# Single-level "keys" merges
###############################################################################

def prepare_keys_inheritance_one_level_with_merge(table):
    if not isinstance(table, dict):
        return
    for k, v in list(table.items()):
        if k == "keys" and isinstance(v, dict):
            if "INHERIT" in v:
                inherits = v["INHERIT"]
                if not isinstance(inherits, list):
                    raise RuntimeError("[keys].INHERIT must be a list.")
                for inh in inherits:
                    parts = inh.split(".")
                    last = parts[-1]
                    if last in v and isinstance(v[last], dict):
                        if "INHERIT" not in v[last]:
                            v[last]["INHERIT"] = []
                        v[last]["INHERIT"].append(inh)
                    else:
                        v[last] = {"INHERIT": [inh]}
                del v["INHERIT"]
        elif isinstance(v, dict):
            prepare_keys_inheritance_one_level_with_merge(v)

###############################################################################
# Build Key
###############################################################################

def build_blueprint_key(full_key, data):
    if "value_type" not in data:
        raise RuntimeError(f"Missing 'value_type' in key: {full_key}")
    if "value_format" not in data:
        raise RuntimeError(f"Missing 'value_format' in key: {full_key}")
    if "value_N" not in data:
        raise RuntimeError(f"Missing 'value_N' in key: {full_key}")

    bk = BlueprintKey()
    bk.keyName = full_key

    vt = data["value_type"]
    bk.value_type = vt if isinstance(vt, list) else [vt]
    vf = data["value_format"]
    bk.value_format = vf if isinstance(vf, list) else [vf]
    vN = data["value_N"]
    bk.value_N = vN if isinstance(vN, list) else [vN]

    bk.description = data.get("description", "")
    bk.longDescription = data.get("long_description", "")
    ex_val = data.get("examples", [])
    if isinstance(ex_val, str):
        bk.examples = [ex_val]
    else:
        bk.examples = ex_val

    def_val = data.get("default_value", [])
    if isinstance(def_val, str):
        bk.default_value = [def_val]
    else:
        bk.default_value = def_val

    bk.cli_long_flag = data.get("cli_long_flag", "")
    bk.cli_short_flag = data.get("cli_short_flag", "")
    bk.cli_excludes = data.get("cli_excludes", [])
    bk.cli_needs = data.get("cli_needs", [])
    bk.cli_required = bool(data.get("cli_required", False))
    bk.retired = dict(data.get("retired", {}) or {})

    return bk

###############################################################################
# Build ALLOWED_KEYS/INTERNAL_KEYS
###############################################################################

def build_blueprint_map(toml_data, table_name):
    result = {}
    if table_name not in toml_data:
        return result
    subtable = toml_data[table_name]
    if not isinstance(subtable, dict):
        return result

    for item_key, item_val in subtable.items():
        full_key = f"{table_name}.{item_key}".lower()
        bk = build_blueprint_key(full_key, item_val)
        result[bk.keyName] = bk

    return result

###############################################################################
# Parse [TASKS.*]
###############################################################################

def parse_task_rec(full_task_name, node, out_map):
    has_desc = ("description" in node)
    has_long = ("long_description" in node)
    has_ex   = ("examples" in node)

    if has_desc and has_long and has_ex:
        bt = BlueprintTask()
        bt.taskName = full_task_name
        bt.description = node["description"]
        bt.longDescription = node["long_description"]
        ex_val = node["examples"]
        bt.examples = ex_val if isinstance(ex_val, list) else [ex_val]

        # parse direct keys
        if "keys" in node and isinstance(node["keys"], dict):
            for ck, cv in node["keys"].items():
                dotted = f"{full_task_name}.{ck}".lower()
                bkey = build_blueprint_key(dotted, cv)
                bt.blueprintKeys.append(bkey)

        # parse groups
        if "groups" in node and isinstance(node["groups"], dict):
            for g_name, g_val in node["groups"].items():
                bg = BlueprintKeyGroup()
                bg.groupName = g_name.lower()

                dval = g_val.get("description", "")
                if isinstance(dval, list):
                    bg.description = dval
                elif isinstance(dval, str):
                    bg.description = [dval] if dval else []

                bg.cli_required = bool(g_val.get("cli_required", False))
                bg.requiredMin = g_val.get("required_min", 1)
                bg.requiredMax = g_val.get("required_max", 1)
                bg.cli_excludes = g_val.get("cli_excludes", [])
                bg.cli_needs = g_val.get("cli_needs", [])

                # group keys
                if "keys" in g_val and isinstance(g_val["keys"], dict):
                    for gk, gv in g_val["keys"].items():
                        dotted_g = f"{full_task_name}.{g_name}.{gk}".lower()
                        grp_bk = build_blueprint_key(dotted_g, gv)
                        bg.blueprintKeys.append(grp_bk)

                bt.groups.append(bg)

        out_map[full_task_name] = bt

    # Recurse sub-commands
    if isinstance(node, dict):
        for child_k, child_v in node.items():
            if child_k in ("description","long_description","examples","keys","groups"):
                continue
            if isinstance(child_v, dict):
                sub_name = (f"{full_task_name}.{child_k}".lower()) if full_task_name else child_k.lower()
                parse_task_rec(sub_name, child_v, out_map)

def build_blueprint_tasks_map(toml_data, root_table_name="TASKS"):
    tmap = {}
    if root_table_name not in toml_data:
        return tmap
    tasks_root = toml_data[root_table_name]
    if not isinstance(tasks_root, dict):
        return tmap

    for k, v in tasks_root.items():
        if isinstance(v, dict):
            top_name = k.lower()
            parse_task_rec(top_name, v, tmap)
    return tmap

###############################################################################
# Build top-level TADAH
###############################################################################

def build_blueprint_tadah(toml_data):
    if "TADAH" not in toml_data:
        raise RuntimeError("Missing [TADAH] in TOML.")
    tad = toml_data["TADAH"]
    if not isinstance(tad, dict):
        raise RuntimeError("[TADAH] must be a table.")

    req = ["logo","version","website","description","long_description","examples"]
    for r in req:
        if r not in tad:
            raise RuntimeError(f"[TADAH] is missing mandatory field '{r}'")

    btt = BlueprintTadah()
    btt.logo = tad["logo"]
    btt.version = tad["version"]
    btt.website = tad["website"]
    btt.description = tad["description"]
    btt.longDescription = tad["long_description"]
    exv = tad["examples"]
    btt.examples = exv if isinstance(exv, list) else [exv]

    if "keys" in tad and isinstance(tad["keys"], dict):
        for kk, vv in tad["keys"].items():
            dotted_key = f"tadah.{kk}".lower()
            nbk = build_blueprint_key(dotted_key, vv)
            btt.blueprintKeys.append(nbk)

    return btt

###############################################################################
# parse_blueprint
###############################################################################

def parse_blueprint(toml_path):
    with open(toml_path, "r", encoding="utf-8") as f:
        data = toml.load(f)

    prepare_keys_inheritance_one_level_with_merge(data)
    data_copy = deepcopy(data)
    expand_inheritance(data_copy, data)

    allowed_map = build_blueprint_map(data_copy, "ALLOWED_KEYS")
    internal_map = build_blueprint_map(data_copy, "INTERNAL_KEYS")
    tasks_map = build_blueprint_tasks_map(data_copy, "TASKS")
    blueprint_tadah = build_blueprint_tadah(data_copy)
    return allowed_map, internal_map, tasks_map, blueprint_tadah


###############################################################################
# Build nested tasks
###############################################################################

def build_nested_task_hierarchy(tasks_map):
    tree = {}

    def insert_path(parts, blueprint_task):
        cur = tree
        for p in parts:
            if p not in cur:
                cur[p] = {}
            cur = cur[p]
        cur["__TASKDATA__"] = blueprint_task

    for dotted_name, tsk in tasks_map.items():
        path = dotted_name.split(".")
        insert_path(path, tsk)

    def build_nodes(name, subdict):
        node_data = subdict.get("__TASKDATA__")
        children = []
        for ck, cv in subdict.items():
            if ck == "__TASKDATA__":
                continue
            children.append(build_nodes(ck, cv))
        children.sort(key=lambda x: x["name"])
        return {"name": name, "data": node_data, "children": children}

    top_nodes = []
    for k, v in tree.items():
        top_nodes.append(build_nodes(k, v))
    top_nodes.sort(key=lambda x: x["name"])
    return top_nodes


###############################################################################
# create_command_block
###############################################################################

def create_command_block(task_obj, cmd_name):
    lines = []
    if task_obj.description.strip():
        lines.append("**DESCRIPTION**:")
        lines.append(f"  {task_obj.description.strip()}")
        lines.append("")
    if task_obj.longDescription.strip():
        lines.append("**LONG DESCRIPTION**:")
        for lln in literal_text_lines(task_obj.longDescription):
            lines.append(f"  {lln}" if lln else "")
        lines.append("")

    # Now "Options" for blueprintKeys
    if task_obj.blueprintKeys:
        lines.append("**OPTIONS**:")
        for bk in task_obj.blueprintKeys:
            lines.extend(build_key_info(bk))
        lines.append("")

    # Next, groups => "OPTIONS::<group>"
    if task_obj.groups:
        for grp in task_obj.groups:
            lines.append(f"**OPTIONS::{grp.groupName}**")
            if grp.description:
                for dline in grp.description:
                    lines.append(f"  {dline.strip()}")
            for bk in grp.blueprintKeys:
                lines.extend(build_key_info(bk))
            lines.append("")

    # examples => bullet-list style
    if task_obj.examples:
        lines.append("**EXAMPLES**:")
        for ex_item in task_obj.examples:
            for eLn in ex_item.splitlines():
                eLn = eLn.strip()
                lines.append(f"  - {eLn}")
        lines.append("")

    return "\n".join(lines)

def literal_text_lines(text):
    """The lines of a blueprint long_description as this page shows them.

    The text is RST (the configuration-keys page renders it), but here it is
    shown inside a command's parsed-literal block, as text: the blank lines
    around it are left out and those inside it are empty; the indentation
    all its lines share is removed and the rest kept, so a layout stays
    nested; and the RST marker of a literal block, '::' closing a paragraph,
    is shown as RST shows it: 'text::' as 'text:', 'text ::' as 'text', and
    a paragraph of '::' alone, with the blank line after it, not at all.
    Inside the literal block it opens (the lines indented more than that
    paragraph, and blank lines), '::' is text.
    """
    lines = [ln.rstrip() for ln in textwrap.dedent(text).splitlines()]
    while lines and not lines[0]:
        lines.pop(0)
    while lines and not lines[-1]:
        lines.pop()
    out = []
    literal_indent = None   # indentation of the paragraph that opened one
    skip_blank = False
    for i, ln in enumerate(lines):
        if skip_blank and not ln:
            skip_blank = False
            continue
        skip_blank = False
        indent = len(ln) - len(ln.lstrip())
        if literal_indent is not None:
            if not ln or indent > literal_indent:
                out.append(ln)
                continue
            literal_indent = None
        closes_paragraph = i + 1 == len(lines) or not lines[i + 1]
        if closes_paragraph and ln.endswith("::"):
            literal_indent = indent
            if ln.strip() == "::":
                skip_blank = True
                continue
            ln = ln[:-3].rstrip() if ln.endswith(" ::") else ln[:-1]
        out.append(ln)
    return out

def build_key_info(bk: BlueprintKey):
    out = []

    # 0) is it bool?
    is_bool = (bk.value_type and bk.value_type[0].upper() == "BOOL")

    # 1) Flags
    flags = []
    if bk.cli_long_flag:
        flags.append(bk.cli_long_flag)
    if bk.cli_short_flag:
        flags.append(bk.cli_short_flag)
    if not flags:
        flags = [bk.keyName]
    fl_str = ", ".join(flags)

    firstLine=f"  {fl_str}  "
    fillStr = " " * (len(firstLine))

    # create spaced filled string of size firstLine
    if not is_bool and bk.value_format:
        valueFormatStr = ""
        i=0
        for vf in bk.value_format:
            if i == 0:
                valueFormatStr += f"{vf.strip()}"
                i += 1
            else:
                valueFormatStr += f"\n{fillStr}{vf.strip()}"
                i += 1
        firstLine += f"{valueFormatStr}"
    out.append(firstLine)

    out.append(f"    {bk.description.strip()}")

    # A retired key: its option is kept only to give the conversion.
    if bk.retired:
        def one(field):
            return " ".join(str(bk.retired.get(field, "")).split())
        out.append(f"    **Retired**: {one('reason')}")
        out.append(f"    **Replaced by**: {one('replacement')}")
        out.append(f"    **Conversion**: {one('conversion')}")
        out.append("")
        return out



    if bk.cli_required:
        out.append("    Required: true")

    # if not bool, show min/max
    if not is_bool and bk.value_N:
        min_str = "1"
        max_str = "1"
        if len(bk.value_N) == 1:
            maxint = abs(bk.value_N[0])
            max_str = "MAX_INT" if maxint == 2147483647 else str(maxint)
        elif len(bk.value_N) == 2:
            min_str = abs(bk.value_N[0])
            max_abs = abs(bk.value_N[1])
            max_str = "MAX_INT" if max_abs == 2147483647 else str(max_abs)
        out.append(f"    **Number of arguments**: Min {min_str}, Max {max_str}")

    # needs
    if bk.cli_needs:
        out.append(f"    **Needs**: {', '.join(bk.cli_needs)}")

    # excludes
    if bk.cli_excludes:
        out.append(f"    **Excludes**: {', '.join(bk.cli_excludes)}")

    # default
    if bk.default_value:
        joineddef = ", ".join(bk.default_value)
        out.append(f"    **Default**: {joineddef}")

    # long description
    if bk.longDescription:
        out.append(f"    **Description**:")
        for lln in literal_text_lines(bk.longDescription):
            out.append(f"      {lln}" if lln else "")

    # examples => bullet list for non-bool
    if (not is_bool) and bk.examples:
        out.append(f"    **Examples**:")
        for eitem in bk.examples:
            for ln in eitem.splitlines():
                out.append(f"      - {ln.strip()}")

    out.append("")
    return out


###############################################################################
# generate_cli_rst
###############################################################################

def generate_cli_rst(tasks_map, allowed_map, output_path):
    """
    same approach: each command is rendered as a heading followed by::
    then the block from create_command_block
    """
    tree = build_nested_task_hierarchy(tasks_map)

    lines = []

    def write_command(node, depth=1, path_names=None):
        if path_names is None:
            path_names = []
        path_names.append(node["name"])
        heading_text = " ".join(path_names)

        underline_char = ["=", "-", "~", "^", '"'][min(depth, 4)]
        lines.append(heading_text)
        lines.append(underline_char * len(heading_text))
        lines.append("")

        tdata = node.get("data", None)
        if tdata:
            block_text = create_command_block(tdata, heading_text)
            lines.append(".. parsed-literal::")
            lines.append("")
            for lline in block_text.splitlines():
                lines.append(f"    {lline}")
            lines.append("")

        for c in node["children"]:
            write_command(c, depth+3, path_names.copy())

        path_names.pop()

    for top_node in tree:
        write_command(top_node, 2, [])

    with open(output_path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))

    print(f"CLI reference successfully written to {output_path}")


if __name__ == "__main__":
    # conf.py runs this script from docs/source; importing it runs nothing.
    toml_path = "config/blueprint_tadah.toml"
    output_path = "cli/cli_auto_gen.rst"
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    allowedMap, internalMap, tasksMap, blueprintTadah = parse_blueprint(toml_path)
    generate_cli_rst(tasksMap, allowedMap, output_path)
