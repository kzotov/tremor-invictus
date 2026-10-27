"""Preview our skins (misc/art/skins.py) on the models the player has: reads
the generated shaders, lays each stage over the model's own textures the way
the renderer blends them, and draws the champion from a few sides.

  python3 misc/tools/skin-preview.py OUT.png SKINS_DIR PACKS... \\
      --model mynx/nyx [--head mynx/nyx] [--variants "" _red _blue]

SKINS_DIR is the folder skins.py wrote. PACKS are folders of .pk3 files or
of loose files (models/players/...); later ones win. Needs numpy and Pillow.
"""
import argparse, importlib.util, io, os, re
import numpy as np
from PIL import Image, ImageDraw

spec = importlib.util.spec_from_file_location("combos", os.path.join(os.path.dirname(os.path.abspath(__file__)), "model-combos.py"))
combos = importlib.util.module_from_spec(spec)
spec.loader.exec_module(combos)


class Files(combos.Packs):
    """Packs plus loose files, searched in the order given."""

    def __init__(self, folders):
        self.files, self.loose = {}, {}
        for folder in folders:
            for name in sorted(os.listdir(folder)):
                if name.lower().endswith(".pk3"):
                    z = __import__("zipfile").ZipFile(os.path.join(folder, name))
                    for n in z.namelist():
                        self.files[n.lower()] = z
                        self.loose.pop(n.lower(), None)
            for root, _, names in os.walk(folder, followlinks=True):
                for n in names:
                    rel = os.path.relpath(os.path.join(root, n), folder).replace(os.sep, "/").lower()
                    if not rel.endswith(".pk3"):
                        self.loose[rel] = os.path.join(root, n)
                        self.files.pop(rel, None)

    def read(self, path):
        p = self.loose.get(path.lower())
        if p:
            return open(p, "rb").read()
        return combos.Packs.read(self, path)

    def rgba(self, path, size=None):
        base = os.path.splitext(path)[0]
        for ext in (".tga", ".jpg", ".png"):
            d = self.read(base + ext)
            if d:
                im = Image.open(io.BytesIO(d)).convert("RGBA")
                if size:
                    im = im.resize(size, Image.BILINEAR)
                return np.asarray(im).astype(float) / 255


def shaders(text):
    out = {}
    text = re.sub(r"//[^\n]*", "", text)
    for name, body in re.findall(r"(\S+)\s*\{((?:[^{}]*\{[^{}]*\})*[^{}]*)\}", text):
        stages = []
        for st in re.findall(r"\{([^{}]*)\}", body):
            m = re.search(r"\bmap\s+(\S+)", st)
            b = re.search(r"blendFunc\s+(\S+)\s+(\S+)", st)
            stages.append((m.group(1) if m else None, b.groups() if b else None, "wave" in st))
        out[name.lower()] = stages
    return out


def surface_images(files, table, name, size=256):
    """(albedo, glow) for a shader or image name, at size x size."""
    stages = table.get(os.path.splitext(name)[0].lower())
    if not stages:
        im = files.rgba(name, (size, size))
        im = im if im is not None else np.full((size, size, 4), 0.5)
        return im[..., :3], None
    col, glow = None, None
    for path, blend, wave in stages:
        im = files.rgba(path, (size, size))
        if im is None:
            continue
        if blend is None:
            col = im[..., :3].copy()
        elif blend == ("GL_DST_COLOR", "GL_SRC_COLOR"):
            col = np.clip(col * im[..., :3] * 2, 0, 1)
        elif blend == ("GL_SRC_ALPHA", "GL_ONE_MINUS_SRC_ALPHA"):
            a = im[..., 3:]
            col = col * (1 - a) + im[..., :3] * a
        elif blend == ("GL_ONE", "GL_ONE"):
            glow = (glow if glow is not None else 0) + im[..., :3] * (0.85 if wave else 1)
    return col, glow


def gather(files, table, body, head, variant):
    bname, _, bskin = body.partition("/")
    hname, _, hskin = head.partition("/")
    bskin, hskin = bskin or "default", hskin or "default"
    parts = {}
    for part, model, skin in (("lower", bname, bskin), ("upper", bname, bskin), ("head", hname, hskin)):
        if part == "head" and variant:
            sk = files.read("models/players/%s/%s/head%s.skin" % (model, skin, variant))
        else:
            sk = files.read("models/players/%s/%s_%s%s.skin" % (model, part, skin, variant))
        parts[part] = combos.read_md3(files.read("models/players/%s/%s.md3" % (model, part))), combos.read_skin(sk)
    legs_f, torso_f = combos.idle_frames(files.read("models/players/%s/animation.cfg" % bname))
    place = lambda p, t: (p[0] + t[0] @ p[1], t[1] @ p[1])
    I = (np.zeros(3), np.eye(3))
    (lt, ls), lsk = parts["lower"]
    (ut, us), usk = parts["upper"]
    (ht, hs), hsk = parts["head"]
    torso = place(I, lt[legs_f]["tag_torso"])
    headf = place(torso, ut[torso_f]["tag_head"])
    meshes, heads, cache = [], [], {}
    for (surfs, skin), fr, frame, part in (((ls, lsk), legs_f, I, "lower"), ((us, usk), torso_f, torso, "upper"),
                                          ((hs, hsk), 0, headf, "head")):
        for name, shader, tris, st, xyz, n in surfs:
            tex = skin.get(name, shader)
            if not tex or "null" in tex.lower() or "nodraw" in tex.lower():
                continue
            if tex not in cache:
                cache[tex] = surface_images(files, table, tex)
            col, glow = cache[tex]
            m = (frame[0] + xyz[fr] @ frame[1], n[fr] @ frame[1], tris, st, col, glow)
            meshes.append(m)
            if part == "head":
                heads.append(m)
    return meshes, heads


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("skins")
    ap.add_argument("packs", nargs="+")
    ap.add_argument("--model", required=True)
    ap.add_argument("--head")
    ap.add_argument("--variants", nargs="+", default=[""])
    ap.add_argument("--yaws", nargs="+", type=float, default=[-30, 60, 180])
    ap.add_argument("--size", type=int, default=320)
    a = ap.parse_args()
    files = Files(a.packs + [a.skins])
    table = {}
    for root, _, names in os.walk(os.path.join(a.skins, "scripts")):
        for n in names:
            if n.endswith(".shader"):
                table.update(shaders(open(os.path.join(root, n)).read()))
    W, H = a.size, int(a.size * 1.3)
    cols = len(a.yaws) + 1
    sheet = Image.new("RGB", (W * cols, (H + 18) * len(a.variants)), (20, 20, 22))
    draw = ImageDraw.Draw(sheet)
    for r, variant in enumerate(a.variants):
        meshes, heads = gather(files, table, a.model, a.head or a.model, variant)
        tiles = [combos.render(meshes, W, H, yaw) for yaw in a.yaws] + [combos.render(heads, W, H, -25, 4)]
        for c, t in enumerate(tiles):
            sheet.paste(Image.fromarray((t * 255).astype(np.uint8)), (c * W, r * (H + 18) + 18))
        draw.text((6, r * (H + 18) + 3), "%s%s" % (a.model, variant or ""), fill=(230, 230, 230))
    sheet.save(a.out)


if __name__ == "__main__":
    main()
