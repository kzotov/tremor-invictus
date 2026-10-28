#!/usr/bin/env python3
"""Import finished champion art into one private local pack.

  python3 misc/art/import-champion-art.py --roster ROSTER.pk3 --roster-cfg OVERRIDES.cfg \
      --art ART_DIR [--shaders OLD_SKINS.pk3 ...] --out PACK.pk3 [--check]

ROSTER.pk3     the champions' player models and skins (models/players/..., scripts/...)
OVERRIDES.cfg  its `set g_championModels "..."` line (the champion -> model mapping)
ART_DIR        <champion>/portrait.png (square, the HUD's three-quarter view), icon.png (square,
               face only), portrait_select.png (square, closer, for champion selection) and
               card.png (2:3, full body)
--shaders      packs whose scripts/*.shader define shaders that the roster's skins name
               (cut-outs, glows, reflections) but the roster pack itself lacks: those
               definitions are copied, nothing else
OUT            a pack holding the roster's files, the needed shaders and
                 gfx/champions/<champion>.tga        the portrait, background cut out (alpha)
                 gfx/champions/<champion>_small.tga  the icon, background cut out (alpha)
                 gfx/champions/<champion>_select.tga the champion-select portrait, as painted
                 gfx/champions/<champion>_card.tga   the full-body card, as painted
               plus import.json (sources, sizes, hashes, mapping) inside the pack.

PNG pixels go into TGA unchanged (no resizing or recolouring); portraits and icons only
gain an alpha channel: the flat dark background around the figure, found by growing from
the border through smooth, background-coloured pixels, becomes transparent so the HUD shows
the head without a box. The art and the roster stay outside the repository; this script
only reads them and writes the private pack."""
import argparse, hashlib, io, json, os, re, sys, zipfile
import numpy as np
from PIL import Image

# source picture, pack suffix, background cut to alpha (HUD portrait and icon, at the
# user's request: the HUD shows the head without a box); the select portrait and the
# card keep their painted dark backgrounds
KINDS = (("portrait", "", True), ("icon", "_small", True), ("portrait_select", "_select", False),
         ("card", "_card", False))


def cut_out(rgb, frame=6, near=42, smooth=6):
    """Alpha for a figure on a flat dark background: 0 on the background that reaches the
    border through low-gradient pixels close to the border colour, 255 on the figure."""
    a = rgb.astype(np.int16)
    h, w, _ = a.shape
    border = np.concatenate([a[:frame].reshape(-1, 3), a[-frame:].reshape(-1, 3),
                             a[:, :frame].reshape(-1, 3), a[:, -frame:].reshape(-1, 3)])
    bg = np.median(border, 0)
    close = np.abs(a - bg).max(2) <= near
    grad = np.zeros((h, w), np.int16)
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        s = np.roll(np.roll(a, dy, 0), dx, 1)
        grad = np.maximum(grad, np.abs(a - s).max(2))
    cand = close & (grad <= smooth)
    m = np.zeros((h, w), bool)
    # seeds on the top and the sides: the figure is cut by the bottom edge
    m[0, :] = cand[0, :]; m[:, 0] = cand[:, 0]; m[:, -1] = cand[:, -1]
    while True:
        g = m.copy()
        g[1:] |= m[:-1]; g[:-1] |= m[1:]; g[:, 1:] |= m[:, :-1]; g[:, :-1] |= m[:, 1:]
        g &= cand
        if (g == m).all():
            break
        m = g
    # the thin rim between background and figure (anti-aliased edge pixels) goes too,
    # where it is still dark and close to the background
    rim = np.zeros_like(m)
    rim[1:] |= m[:-1]; rim[:-1] |= m[1:]; rim[:, 1:] |= m[:, :-1]; rim[:, :-1] |= m[:, 1:]
    m |= rim & close
    alpha = np.where(m, 0, 255).astype(np.float32)
    # soften the edge by one pixel
    k = alpha.copy()
    k[1:-1, 1:-1] = (alpha[1:-1, 1:-1] * 4 + alpha[:-2, 1:-1] + alpha[2:, 1:-1] + alpha[1:-1, :-2] + alpha[1:-1, 2:]) / 8
    return np.minimum(alpha, np.maximum(k, alpha * 0)).astype(np.uint8), float(m.mean())


def tga_bytes(im):
    b = io.BytesIO()
    im.save(b, "TGA", rle=True)
    return b.getvalue()


def shader_blocks(text):
    """name -> full text of each top-level shader definition."""
    out, i, n = {}, 0, len(text)
    text = re.sub(r"//[^\n]*", "", text)
    for mt in re.finditer(r"^[ \t]*([^\s{}]+)\s*\{", text, re.M):
        if mt.start() < i:
            continue
        depth, j = 0, mt.end() - 1
        while j < len(text):
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        out.setdefault(mt.group(1).lower(), text[mt.start():j + 1].strip() + "\n")
        i = j + 1
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--roster", required=True)
    ap.add_argument("--roster-cfg", required=True)
    ap.add_argument("--art", required=True)
    ap.add_argument("--shaders", nargs="*", default=[])
    ap.add_argument("--out", required=True)
    ap.add_argument("--preview", help="folder for alpha proof sheets (optional)")
    args = ap.parse_args()

    line = [l for l in open(args.roster_cfg).read().splitlines() if l.strip().startswith("set g_championModels")]
    mapping = dict(p.split("=", 1) for p in re.search(r'"(.*)"', line[-1]).group(1).split())
    report = dict(roster=args.roster, roster_sha256=hashlib.sha256(open(args.roster, "rb").read()).hexdigest(),
                  mapping=mapping, art={}, shaders_added=[], warnings=[])
    rz = zipfile.ZipFile(args.roster)
    names = rz.namelist()
    # every texture/shader name the roster's skins use
    used = set()
    for n in names:
        if n.endswith(".skin"):
            for l in rz.read(n).decode("latin-1").splitlines():
                if "," in l:
                    t = l.split(",", 1)[1].strip()
                    if t:
                        used.add(re.sub(r"\.(tga|jpg|png)$", "", t, flags=re.I).lower())
    have = {}
    for n in names:
        if n.startswith("scripts/") and n.endswith(".shader"):
            have.update(shader_blocks(rz.read(n).decode("latin-1")))
    extra = {}
    for p in args.shaders:
        z = zipfile.ZipFile(p)
        for n in z.namelist():
            if n.startswith("scripts/") and n.endswith(".shader"):
                for k, v in shader_blocks(z.read(n).decode("latin-1")).items():
                    if k in used and k not in have and k not in extra:
                        extra[k] = v
    report["shaders_added"] = sorted(extra)
    # every mapped model must be in the roster pack (or be a stock model)
    for champ, spec in mapping.items():
        for k, part in enumerate(spec.split(":")):
            model, _, skin = part.partition("/")
            skin = skin or "default"
            want = f"models/players/{model}/{'head' if k else 'lower'}_{skin}.skin".lower()
            if not any(n.lower() == want for n in names):
                report["warnings"].append(f"{champ}: {model}/{skin} skin not in the roster pack (stock or missing)")

    tmp = args.out + ".tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as z:
        for n in names:
            if not n.endswith("/"):
                z.writestr(n, rz.read(n))
        if extra:
            z.writestr("scripts/zzz-champion-roster-extra.shader", "".join(extra[k] + "\n" for k in sorted(extra)))
        for champ in sorted(os.listdir(args.art)):
            d = os.path.join(args.art, champ)
            if not os.path.isdir(d) or not os.path.exists(os.path.join(d, "portrait.png")):
                continue
            report["art"][champ] = {}
            for kind, suffix, alpha in KINDS:
                src = os.path.join(d, kind + ".png")
                if not os.path.exists(src):
                    report["warnings"].append(f"{champ}: no {kind}.png")
                    continue
                im = Image.open(src)
                rgb = np.asarray(im.convert("RGB"))
                info = dict(source=src, size=list(im.size), mode=im.mode,
                            sha256=hashlib.sha256(open(src, "rb").read()).hexdigest())
                if alpha:
                    a, frac = cut_out(rgb)
                    out = Image.fromarray(np.dstack([rgb, a]), "RGBA")
                    info["transparent_fraction"] = round(frac, 3)
                    if args.preview:
                        os.makedirs(args.preview, exist_ok=True)
                        check = Image.new("RGB", im.size, (40, 160, 70))
                        check.paste(out, (0, 0), out)
                        check.save(os.path.join(args.preview, f"{champ}{suffix or '_portrait'}.png"))
                else:
                    out = Image.fromarray(rgb, "RGB")
                path = f"gfx/champions/{champ}{suffix}.tga"
                data = tga_bytes(out)
                z.writestr(path, data)
                # the pixels in the pack are the source's (alpha added at most)
                back = np.asarray(Image.open(io.BytesIO(data)).convert("RGB"))
                info["pixels_unchanged"] = bool((back == rgb).all())
                info["pack_path"] = path
                report["art"][champ][kind] = info
        z.writestr("import.json", json.dumps(report, indent=1))
    os.replace(tmp, args.out)
    print(json.dumps(dict(out=args.out, champions_with_art=len(report["art"]),
                          files=sum(len(v) for v in report["art"].values()),
                          shaders_added=len(extra), warnings=report["warnings"]), indent=1))


if __name__ == "__main__":
    main()
