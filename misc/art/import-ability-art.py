#!/usr/bin/env python3
"""Put the painted ability-icon pack into a build, or say plainly that it is missing.

  python3 misc/art/import-ability-art.py sync   GENERATED [--pack PACK]
  python3 misc/art/import-ability-art.py status GENERATED

The pack (invictus-ability-gradients-v1: manifest.json, runtime-checksums.json,
runtime/) is art and stays outside the code-only repository, by default in
$TREMOR_LOCAL/branding/invictus-ability-gradients-v1 (TREMOR_LOCAL defaults to
~/projects/quake/local), or wherever TREMOR_ABILITY_ART or --pack points. It is a
separate pack from the UI branding (import-brand-art.py). The build runs `sync` at
configure time, after the art scripts (cmake/identity.cmake), and again on every
build (target tremor-ability-art); the run, test and package scripts run it too.

The game reads loose files before any pk3 in the same folder, so the drawn icons in
GENERATED would hide a pk3 of the pack: the pack is copied over them instead.

sync     checks every file of runtime-checksums.json (SHA-256, a 128 x 128 32-bit
         TGA, a path inside the pack) and that every active ability of
         code/game/bg_misc.c has its two glyphs, then copies the 60 textures into
         GENERATED (gfx/tremor/icons/abilities, gfx/tremor/abilities and
         gfx/abilities/charges), keeping the drawn icons each replaces in
         GENERATED/ability-art/drawn and listing what it put in with its SHA-256 in
         GENERATED/ability-art/installed.txt. Only files that differ are written.
         With the pack's alignment/anchors.json it also writes
         GENERATED/gfx/tremor/icons/abilities/anchors.txt: for each glyph whose
         installed texture has the SHA-256 its anchor was measured on, the point
         the HUD puts on the ring's centre (the rest stay centred).
         Without the pack, or with a broken one, it puts the drawn icons back where
         an earlier sync replaced them (and removes the textures nothing draws:
         the charge icons and the small glyphs), so a build never keeps a stale
         copy, and prints a warning. Exit status 0 without a pack, 1 with a broken
         one.
status   prints "painted <pack>" and exits 0 when GENERATED holds every pack texture
         unchanged, else prints "drawn" and exits 3 (scripts/package.sh).

Plain Python 3, no packages.
"""
import argparse, hashlib, json, os, re, shutil, struct, sys

PACK_NAME = "invictus-ability-gradients-v1"
STATE = "ability-art"                       # in GENERATED, never packed
INSTALLED = STATE + "/installed.txt"
DRAWN = STATE + "/drawn"
ANCHORS = "gfx/tremor/icons/abilities/anchors.txt"     # read by the HUD
ANCHORS_JSON = "alignment/anchors.json"
# names the art scripts used before an ability's key was fixed
OBSOLETE = ("gfx/tremor/icons/abilities/eternal.tga", "gfx/tremor/abilities/eternal.tga")


def warn(text):
    print("import-ability-art: " + text, file=sys.stderr)


def default_pack():
    if os.environ.get("TREMOR_ABILITY_ART"):
        return os.environ["TREMOR_ABILITY_ART"]
    local = os.environ.get("TREMOR_LOCAL") or os.path.join(os.path.expanduser("~"), "projects", "quake", "local")
    return os.path.join(local, "branding", PACK_NAME)


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def ability_keys():
    """The cvar names of bg_abilityInfo in code/game/bg_misc.c, in order."""
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "code", "game", "bg_misc.c")
    with open(src, encoding="utf-8", errors="replace") as f:
        text = f.read()
    start = text.find("bg_abilityInfo[")
    body = text[start:text.find("};", start)]
    return [k for k in re.findall(r'^\s*\{\s*"([a-z_]+)"', body, re.M) if k != "none"]


def check_tga(path):
    """None when the file is a 128 x 128 32-bit TGA, else a reason."""
    with open(path, "rb") as f:
        head = f.read(18)
    if len(head) < 18 or head[2] not in (2, 10) or head[16] != 32:
        return "not a 32-bit TGA"
    if struct.unpack("<HH", head[12:16]) != (128, 128):
        return "not 128 x 128"
    return None


def check_pack(pack):
    """({name: sha256}, problems): the pack's textures, and what is wrong with it."""
    problems, files = [], {}
    for name in ("manifest.json", "runtime-checksums.json"):
        if not os.path.isfile(os.path.join(pack, name)):
            problems.append("no " + name)
    if problems:
        return files, problems
    with open(os.path.join(pack, "runtime-checksums.json")) as f:
        sums = json.load(f)
    for name, digest in sorted(sums.items()):
        parts = name.split("/")
        if name.startswith("/") or ".." in parts or not name.startswith("gfx/") or not name.endswith(".tga"):
            problems.append("unsafe path " + name)
            continue
        path = os.path.join(pack, "runtime", name)
        if not os.path.isfile(path):
            problems.append("missing runtime/" + name)
        elif sha256(path) != digest:
            problems.append("runtime/%s: checksum mismatch" % name)
        else:
            bad = check_tga(path)
            if bad:
                problems.append("runtime/%s: %s" % (name, bad))
            else:
                files[name] = digest
    keys = ability_keys()
    if not keys:
        problems.append("no abilities found in code/game/bg_misc.c")
    for key in keys:
        for d in ("gfx/tremor/icons/abilities/", "gfx/tremor/abilities/"):
            if d + key + ".tga" not in sums:
                problems.append("no %s%s.tga for the ability %s" % (d, key, key))
    return files, problems


def read_installed(generated):
    """{name: sha256} the last sync put in, empty when none."""
    path = os.path.join(generated, INSTALLED)
    entries = {}
    if os.path.isfile(path):
        with open(path) as f:
            for line in f:
                parts = line.split()
                if len(parts) == 2 and parts[1].startswith("gfx/"):
                    entries[parts[1]] = parts[0]
    return entries


def installed(generated):
    entries = read_installed(generated)
    return bool(entries) and all(
        os.path.isfile(os.path.join(generated, n)) and sha256(os.path.join(generated, n)) == d for n, d in entries.items())


def write_if_changed(path, data):
    if os.path.isfile(path):
        with open(path, "rb") as f:
            if f.read() == data:
                return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    return True


def revert(generated):
    """Put back what the last sync replaced; the drawn icons when there are any."""
    entries = read_installed(generated)
    if not entries:
        return 0
    restored = removed = 0
    for name, digest in entries.items():
        path = os.path.join(generated, name)
        if os.path.isfile(path) and sha256(path) != digest:
            continue        # the art scripts drew it again since
        drawn = os.path.join(generated, DRAWN, name)
        if os.path.isfile(drawn):
            shutil.copy2(drawn, path)
            restored += 1
        elif os.path.isfile(path):
            os.remove(path)
            removed += 1
    os.remove(os.path.join(generated, INSTALLED))
    warn("put back %d drawn icon%s and removed %d painted texture%s an earlier sync had put in"
         % (restored, "" if restored == 1 else "s", removed, "" if removed == 1 else "s"))
    return restored + removed


def anchor_lines(generated, pack):
    """The text of ANCHORS: the pack's measured glyph anchors (alignment/anchors.json),
    one "<key> <u> <v>" line for each ability whose installed glyphs (both paths) are
    the very textures the anchor was measured on; None when the pack has none. The HUD
    centres any glyph without a line (cg_draw.c CG_AbilityAnchor)."""
    path = os.path.join(pack, ANCHORS_JSON)
    if not os.path.isfile(path):
        warn("%s has no %s: the glyphs are centred by their texture square" % (pack, ANCHORS_JSON))
        return None
    with open(path) as f:
        icons = json.load(f).get("icons", {})
    lines = ["// measured glyph anchors (u v from the top left) for the painted ability icons,",
             "// written by misc/art/import-ability-art.py from %s" % ANCHORS_JSON]
    for key in ability_keys():
        entry = icons.get(key)
        if not entry:
            warn("no anchor for %s: centred" % key)
            continue
        u, v = entry["anchor_uv"]
        digest = entry.get("sha256")
        if not (0 < u < 1 and 0 < v < 1):
            warn("anchor of %s out of range: centred" % key)
            continue
        if not all(os.path.isfile(os.path.join(generated, d + key + ".tga"))
                   and sha256(os.path.join(generated, d + key + ".tga")) == digest
                   for d in ("gfx/tremor/icons/abilities/", "gfx/tremor/abilities/")):
            warn("the installed %s glyph is not the one its anchor was measured on: centred" % key)
            continue
        lines.append("%s %.6f %.6f" % (key, u, v))
    return "\n".join(lines) + "\n"


def sync(generated, pack):
    files, problems = check_pack(pack) if os.path.isdir(pack) else ({}, None)
    if problems is None or problems:
        if problems is None:
            warn("no ability icon pack at %s" % pack)
        for p in problems or ():
            warn("%s: %s" % (pack, p))
        revert(generated)
        warn("the HUD uses the DRAWN placeholder ability icons (misc/art/make-icons.py, make-abilities.py);")
        warn("put the pack there, or set TREMOR_ABILITY_ART to it, then configure or build again")
        return 1 if problems else 0

    changed = 0
    for name in OBSOLETE:
        path = os.path.join(generated, name)
        if os.path.isfile(path):
            os.remove(path)
            changed += 1
    stamp = ["# %s ability icons, copied from %s by misc/art/import-ability-art.py" % (PACK_NAME, pack)]
    for name, digest in sorted(files.items()):
        dest = os.path.join(generated, name)
        # keep the drawn icon this one replaces (redrawn ones too), to put back without the pack
        if os.path.isfile(dest) and sha256(dest) != digest:
            keep = os.path.join(generated, DRAWN, name)
            os.makedirs(os.path.dirname(keep), exist_ok=True)
            shutil.copy2(dest, keep)
        with open(os.path.join(pack, "runtime", name), "rb") as f:
            changed += write_if_changed(dest, f.read())
        stamp.append("%s %s" % (digest, name))
    anchors = anchor_lines(generated, pack)
    if anchors is not None:
        data = anchors.encode()
        changed += write_if_changed(os.path.join(generated, ANCHORS), data)
        stamp.append("%s %s" % (hashlib.sha256(data).hexdigest(), ANCHORS))
    elif os.path.isfile(os.path.join(generated, ANCHORS)):
        os.remove(os.path.join(generated, ANCHORS))
        changed += 1
    write_if_changed(os.path.join(generated, INSTALLED), ("\n".join(stamp) + "\n").encode())
    if changed:
        print("import-ability-art: %s ability icons from %s (%d file%s updated)"
              % (PACK_NAME, pack, changed, "" if changed == 1 else "s"))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=("sync", "status"))
    parser.add_argument("generated", help="a build's generated folder (holds gfx/tremor)")
    parser.add_argument("--pack", default=None, help="the ability icon pack (default: %s)" % default_pack())
    args = parser.parse_args()
    generated = os.path.abspath(args.generated)
    if args.command == "status":
        if installed(generated):
            with open(os.path.join(generated, INSTALLED)) as f:
                print("painted " + f.readline().split(" from ")[-1].split(" by ")[0])
            return 0
        print("drawn")
        return 3
    pack = os.path.abspath(os.path.expanduser(args.pack or default_pack()))
    if not os.path.isdir(os.path.join(generated, "gfx", "tremor")):
        warn("%s has no gfx/tremor yet (configure the build first)" % generated)
    return sync(generated, pack)


if __name__ == "__main__":
    sys.exit(main())
