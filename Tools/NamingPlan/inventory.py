"""NAMING-01 inventory: reflected types, ActorComponent rename candidates, Dx-derived classes.

Read-only. Usage: python -X utf8 -I inventory.py <host-root> <dtdevice-root> <dtcore-root> <out.json>
"""
import json, os, re, sys

host, device, core, out = sys.argv[1:5]
ROOTS = {"Project": os.path.join(host, "Source"), "DTDevice": os.path.join(device, "Source"), "DTCore": os.path.join(core, "Source")}
DECL = re.compile(r"U(CLASS|STRUCT|ENUM|INTERFACE)\s*\((?:[^()]|\([^()]*\))*\)\s*(?:class|struct|enum class|enum)\s+(?:\w+_API\s+)?(\w+)(?:\s*:\s*(?:public\s+)?([\w:]+))?", re.S)
# Engine component bases (anything ending in Component that is not ours is treated as engine).
ENGINE_COMPONENT_ROOT = "UActorComponent"

types = {}
for owner, root in ROOTS.items():
    for dirpath, _, files in os.walk(root):
        for f in files:
            if not f.endswith(".h"):
                continue
            path = os.path.join(dirpath, f)
            text = open(path, encoding="utf-8-sig", errors="ignore").read()
            for m in DECL.finditer(text):
                kind, name, parent = m.group(1), m.group(2), (m.group(3) or "").split("::")[-1]
                types[name] = {"Kind": kind, "Name": name, "Parent": parent, "Owner": owner,
                               "File": os.path.relpath(path, {"Project": host, "DTDevice": device, "DTCore": core}[owner]).replace("\\", "/")}

def chain(name):
    seen = []
    while name in types and name not in seen:
        seen.append(name)
        name = types[name]["Parent"]
    seen.append(name)
    return seen

def is_component(name):
    c = chain(name)
    last = c[-1]
    return ENGINE_COMPONENT_ROOT in c or (last.startswith("U") and last.endswith("Component"))

def dx_root(name):
    for n in chain(name)[1:]:
        if n in types and types[n]["Owner"] == "DTCore" and n[1:].startswith("Dx"):
            return n
    return None

def strip(n):
    return n[1:] if n[:1] in "UAF" and n[1:2].isupper() else n

rows = []
for t in types.values():
    if t["Owner"] == "DTCore" or t["Kind"] != "CLASS":
        continue
    item = dict(t, Chain=chain(t["Name"]))
    if is_component(t["Name"]):
        item["Component"] = True
        if t["Name"].endswith("Component"):
            item["Rename"] = t["Name"][: -len("Component")] + "Comp"
        elif t["Name"].endswith("Comp") or t["Name"].endswith("CompBase"):
            item["Rename"] = None
        else:
            item["Rename"] = t["Name"] + "Comp"
            item["Note"] = "component without Comp/Component suffix"
    root = dx_root(t["Name"])
    if root:
        item["DxRoot"] = root
        base = strip(root)
        base = base[:-4] if base.endswith("Base") else base
        if not strip(t["Name"]).startswith("Dx"):
            # User decisions 2026-11-04: project classes continue the DTCore parent name (UDxWidget<Feature>);
            # DTDevice classes use the UDxDevice family (UDeviceSensorPanelBase -> UDxDeviceSensorPanelBase).
            stem = strip(t["Name"])
            if t["Owner"] == "DTDevice":
                item["DxRename"] = t["Name"][0] + "Dx" + (stem if stem.startswith("Device") else "Device" + stem)
            else:
                item["DxRename"] = t["Name"][0] + base + stem.replace(base[2:], "")
    rows.append(item)

json.dump({"Types": len(types), "Rows": sorted(rows, key=lambda r: (r["Owner"], r["File"]))}, open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
comp = [r for r in rows if r.get("Component")]
print("types", len(types), "classes", len(rows), "components", len(comp),
      "rename", sum(1 for r in comp if r.get("Rename")), "dx-derived", sum(1 for r in rows if r.get("DxRoot")))
