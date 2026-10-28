#!/usr/bin/env python3
"""Put the approved UI branding pack into a build, or say plainly that it is missing.

  python3 misc/art/import-brand-art.py sync   GENERATED [--pack PACK] [--icons DIR]
  python3 misc/art/import-brand-art.py status GENERATED

The pack (tremor-invictus-v1: README.txt, manifest.json, runtime/, icons/) is art and
stays outside the code-only repository, by default in
$TREMOR_LOCAL/branding/tremor-invictus-v1 (TREMOR_LOCAL defaults to ~/projects/quake/local),
or wherever TREMOR_BRAND or --pack points. The build runs `sync` at configure time, after
the art scripts (cmake/identity.cmake), and again on every build (target tremor-brand);
the run, test and package scripts run it too, so a script that redraws the placeholders
never leaves them in place while the pack is there.

sync     copies the pack's six UI textures over the drawn placeholders in
         GENERATED/gfx/tremor (only files that differ, so their dates stay put) and
         records them with their SHA-256 in GENERATED/gfx/tremor/brand.txt.
         With --icons, also stages the app icons in DIR: TremorInvictus.icns (the
         macOS bundle: the approved Dock tile, icons/macos-dock/, else the pack's
         first emblem export), tremor-invictus.ico (Windows executable) and
         tremor_window_icon.h, the 32 x 32 window icon SDL sets on Windows and Linux.
         Without the pack it keeps the placeholders that misc/art and misc/logo draw
         (the old working logo), makes the placeholder logo blend and sit like the
         real one (its black becomes transparent, its artwork moves into the pack's
         margins), and prints a warning; icons staged by an
         earlier sync stay (a container build sees no pack: the host stages them).
         Exit status 0 either way; 1 only when a pack is there but broken (a missing
         or malformed file).
status   prints "approved <pack>" and exits 0 when GENERATED holds the pack's textures
         unchanged, else prints "placeholder" and exits 3 (scripts/package.sh).

Plain Python 3, no packages: PNG and TGA headers are read here, and the two PNGs this
script has to open (the placeholder logo, the 32 px emblem) are decoded with zlib.
"""
import argparse, hashlib, os, struct, sys, zlib

PACK_NAME = "tremor-invictus-v1"
# published path, kind, size; the logo shader blends straight alpha
TEXTURES = (
    ("gfx/tremor/logo.png", "png-rgba", (1024, 270)),
    ("gfx/tremor/emblem.png", "png-rgba", (512, 512)),
    ("gfx/tremor/logo-stacked.png", "png-rgba", (1024, 1024)),
    ("gfx/tremor/splash/back.tga", "tga-rgb", (1024, 512)),        # a 16:9 picture squeezed 2:1
    ("gfx/tremor/splash/nomap.tga", "tga-rgb", (256, 256)),   # a 4:3 picture squeezed square
    ("gfx/tremor/console.tga", "tga-rgb", (1024, 512)),
)
# staged name, then the pack's files for it, the first one there wins
ICONS = (
    ("TremorInvictus.icns", ("icons/macos-dock/TremorInvictus.icns", "icons/TremorInvictus.icns")),
    ("tremor-invictus.ico", ("icons/tremor-invictus.ico",)),
)
WINDOW_ICON = "icons/emblem-32.png"
STAMP = "gfx/tremor/brand.txt"


def warn(text):
    print("import-brand-art: " + text, file=sys.stderr)


def default_pack():
    if os.environ.get("TREMOR_BRAND"):
        return os.environ["TREMOR_BRAND"]
    local = os.environ.get("TREMOR_LOCAL") or os.path.join(os.path.expanduser("~"), "projects", "quake", "local")
    return os.path.join(local, "branding", PACK_NAME)


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def header(path, kind):
    """(width, height) when the file is the kind asked for, else a reason string."""
    with open(path, "rb") as f:
        head = f.read(33)
    if kind == "png-rgba":
        if head[:8] != b"\x89PNG\r\n\x1a\n" or head[12:16] != b"IHDR":
            return "not a PNG"
        w, h, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", head[16:29])
        if depth != 8 or colour != 6 or interlace:
            return "not an 8-bit RGBA PNG"
        return (w, h)
    if len(head) < 18 or head[2] != 2 or head[16] != 24:
        return "not an uncompressed 24-bit TGA"
    return struct.unpack("<HH", head[12:16])


def icon_source(pack, sources):
    for src in sources:
        if os.path.isfile(os.path.join(pack, src)):
            return os.path.join(pack, src)
    return None


def icns_problem(path):
    """Why an .icns would not do for the bundle (no 1024 px image), else None."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"icns" or struct.unpack(">I", data[4:8])[0] != len(data):
        return "not an icns file"
    pos, kinds = 8, set()
    while pos + 8 <= len(data):
        kind, length = struct.unpack(">4sI", data[pos:pos + 8])
        kinds.add(kind)
        pos += max(length, 8)
    return None if b"ic10" in kinds else "no 1024 px image (ic10)"


def check_pack(pack):
    """The problems with the pack, an empty list when it is complete."""
    problems = []
    if not os.path.isfile(os.path.join(pack, "manifest.json")):
        problems.append("no manifest.json")
    for dest, sources in ICONS:
        src = icon_source(pack, sources)
        if not src:
            problems.append("missing " + " or ".join(sources))
        elif dest.endswith(".icns") and icns_problem(src):
            problems.append("%s: %s" % (os.path.relpath(src, pack), icns_problem(src)))
    if not os.path.isfile(os.path.join(pack, WINDOW_ICON)):
        problems.append("missing " + WINDOW_ICON)
    for name, kind, size in TEXTURES:
        path = os.path.join(pack, "runtime", name)
        if not os.path.isfile(path):
            problems.append("missing runtime/" + name)
            continue
        got = header(path, kind)
        if isinstance(got, str):
            problems.append("runtime/%s: %s" % (name, got))
        elif tuple(got) != size:
            problems.append("runtime/%s: %d x %d, expected %d x %d" % (name, got[0], got[1], size[0], size[1]))
    return problems


def read_png(path):
    """width, height, channels and rows of bytes of an 8-bit RGB/RGBA PNG."""
    with open(path, "rb") as f:
        data = f.read()
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        length, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            w, h, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or colour not in (2, 6) or interlace:
                raise ValueError(path + ": not an 8-bit RGB(A) PNG")
            n = 4 if colour == 6 else 3
        elif tag == b"IDAT":
            idat += body
        pos += 12 + length
    raw, stride, rows, prev = zlib.decompress(idat), w * n, [], bytearray(w * n)
    for y in range(h):
        kind, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - n] if i >= n else 0
            b = prev[i]
            c = prev[i - n] if i >= n else 0
            if kind == 1:
                line[i] = (line[i] + a) & 255
            elif kind == 2:
                line[i] = (line[i] + b) & 255
            elif kind == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    return w, h, n, rows


def write_png(path, w, h, rows):
    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xffffffff)
    raw = b"".join(b"\0" + bytes(r) for r in rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


# where the pack's horizontal logo has its artwork in its 1024 x 270 texture; the
# menus place the logo by it (UI_DrawLogo, CG_DrawLogo, .logo in tremor.rcss)
LOGO_ARTWORK = (188, 46, 838, 234)


def unmatte_placeholder_logo(generated):
    """The placeholder logo is drawn opaque on black for additive blending, filling its
    texture. The logo shader blends alpha and the menus expect the pack's margins, so its
    black becomes transparent (alpha = brightest channel, colour divided by it: the same
    picture over black, no black box over anything else) and its artwork is scaled into
    LOGO_ARTWORK, keeping its shape."""
    path = os.path.join(generated, "gfx/tremor/logo.png")
    if not os.path.isfile(path):
        return
    w, h, n, rows = read_png(path)
    if n == 4 and rows[0][3] == 0:
        return      # done before, or the pack's own
    # premultiplied (colour = colour over black) and alpha, and the artwork's bounds
    src, x0, y0, x1, y1 = [], w, h, 0, 0
    for y, row in enumerate(rows):
        for x in range(w):
            r, g, b = row[x * n], row[x * n + 1], row[x * n + 2]
            a = max(r, g, b)
            src.append((r, g, b, a))
            if a > 8:
                x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x + 1), max(y1, y + 1)
    if x1 <= x0:
        return
    bx0, by0, bx1, by1 = LOGO_ARTWORK
    k = min((bx1 - bx0) / (x1 - x0), (by1 - by0) / (y1 - y0))
    ox, oy = bx0, (by0 + by1 - (y1 - y0) * k) / 2
    out = []
    for y in range(h):
        line = bytearray(w * 4)
        sy = (y + 0.5 - oy) / k + y0 - 0.5
        for x in range(w):
            sx = (x + 0.5 - ox) / k + x0 - 0.5
            if sx < x0 - 1 or sx > x1 or sy < y0 - 1 or sy > y1:
                continue
            ix, iy = int(sx // 1), int(sy // 1)
            fx, fy = sx - ix, sy - iy
            acc = [0.0, 0.0, 0.0, 0.0]
            for dx, dy, f in ((0, 0, (1 - fx) * (1 - fy)), (1, 0, fx * (1 - fy)), (0, 1, (1 - fx) * fy), (1, 1, fx * fy)):
                px, py = ix + dx, iy + dy
                if 0 <= px < w and 0 <= py < h and f:
                    p = src[py * w + px]
                    for c in range(4):
                        acc[c] += p[c] * f
            a = acc[3]
            if a >= 1:
                line[x * 4:x * 4 + 4] = bytes((min(255, int(acc[0] * 255 / a)), min(255, int(acc[1] * 255 / a)),
                                               min(255, int(acc[2] * 255 / a)), min(255, int(a + 0.5))))
        out.append(line)
    write_png(path, w, h, out)


def window_icon_header(png, out):
    """The emblem as the GIMP C-source struct sdl_glimp.c sets as the window icon."""
    w, h, n, rows = read_png(png)
    pixels = b"".join(bytes(r) if n == 4 else bytes(v for x in range(w) for v in (*r[x * 3:x * 3 + 3], 255)) for r in rows)
    lines = []
    for i in range(0, len(pixels), 18):
        lines.append('  "' + "".join("\\%03o" % v for v in pixels[i:i + 18]) + '"')
    text = ("/* the Tremor Invictus emblem, %d x %d RGBA: made by misc/art/import-brand-art.py\n"
            "   from the branding pack's %s (art kept outside the repository) */\n\n"
            "static const struct {\n  unsigned int width;\n  unsigned int height;\n"
            "  unsigned int bytes_per_pixel;\n  unsigned char pixel_data[%d * %d * 4 + 1];\n"
            "} CLIENT_WINDOW_ICON = {\n  %d, %d, 4,\n%s,\n};\n") % (w, h, WINDOW_ICON, w, h, w, h, "\n".join(lines))
    write_if_changed(out, text.encode())


def write_if_changed(path, data):
    if os.path.isfile(path):
        with open(path, "rb") as f:
            if f.read() == data:
                return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    return True


def stamp_entries(generated):
    """{name: sha256} recorded by the last sync, empty when none."""
    path = os.path.join(generated, STAMP)
    entries = {}
    if os.path.isfile(path):
        with open(path) as f:
            for line in f:
                parts = line.split()
                if len(parts) == 2 and parts[1].startswith("gfx/"):
                    entries[parts[1]] = parts[0]
    return entries


def installed(generated):
    """True when GENERATED holds all six pack textures unchanged since the last sync."""
    entries = stamp_entries(generated)
    for name, _, _ in TEXTURES:
        path = os.path.join(generated, name)
        if name not in entries or not os.path.isfile(path) or sha256(path) != entries[name]:
            return False
    return True


def sync(generated, pack, icons):
    problems = check_pack(pack) if os.path.isdir(pack) else None
    if problems:
        for p in problems:
            warn("%s: %s" % (pack, p))
    if problems is None or problems:
        if problems is None:
            warn("no branding pack at %s" % pack)
        if installed(generated):
            warn("keeping the approved textures and icons imported before (%s)" % os.path.join(generated, STAMP))
        else:
            unmatte_placeholder_logo(generated)
            warn("menus, loading screen and console use the drawn PLACEHOLDER art (the old working logo),")
            warn("and the app icons the placeholder emblem unless an earlier sync staged the pack's;")
            warn("put the pack there, or set TREMOR_BRAND to it, then configure or build again")
        return 1 if problems else 0

    stamp = ["# %s UI branding, copied from %s by misc/art/import-brand-art.py" % (PACK_NAME, pack)]
    changed = 0
    for name, _, _ in TEXTURES:
        src = os.path.join(pack, "runtime", name)
        with open(src, "rb") as f:
            data = f.read()
        changed += write_if_changed(os.path.join(generated, name), data)
        stamp.append("%s %s" % (hashlib.sha256(data).hexdigest(), name))
    write_if_changed(os.path.join(generated, STAMP), ("\n".join(stamp) + "\n").encode())
    if icons:
        for dest, sources in ICONS:
            with open(icon_source(pack, sources), "rb") as f:
                changed += write_if_changed(os.path.join(icons, dest), f.read())
        window_icon_header(os.path.join(pack, WINDOW_ICON), os.path.join(icons, "tremor_window_icon.h"))
    if changed:
        print("import-brand-art: %s UI branding from %s (%d file%s updated)" % (PACK_NAME, pack, changed, "" if changed == 1 else "s"))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=("sync", "status"))
    parser.add_argument("generated", help="a build's generated folder (holds gfx/tremor)")
    parser.add_argument("--pack", default=None, help="the branding pack (default: %s)" % default_pack())
    parser.add_argument("--icons", default=None, help="sync: also stage the app icons into this folder")
    args = parser.parse_args()
    generated = os.path.abspath(args.generated)
    if args.command == "status":
        if installed(generated):
            with open(os.path.join(generated, STAMP)) as f:
                print("approved " + f.readline().split(" from ")[-1].split(" by ")[0])
            return 0
        print("placeholder")
        return 3
    pack = os.path.abspath(os.path.expanduser(args.pack or default_pack()))
    if not os.path.isdir(os.path.join(generated, "gfx", "tremor")):
        warn("%s has no gfx/tremor yet (configure the build first)" % generated)
    return sync(generated, pack, args.icons and os.path.abspath(args.icons))


if __name__ == "__main__":
    sys.exit(main())
