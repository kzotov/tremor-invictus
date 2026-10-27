"""The form shading of the looks that ask for it (FORM in their module),
worked out on the player's own machine from the player models in the
player's own Quake III data, into a pack that stays in their home folder:
none of it is in the repository, in tremor-assets.pk3 or in any package.

  python3 -I misc/art/bake-form.py BASEQ3 OUT.pk3

BASEQ3 is the folder with the player's pak0.pk3. For each FORM sheet,
md3bake.py works out how much each texel faces the light and how open it
is; the sheet's FORM entry turns them into the two layers its shader
already has (skins.py ships them neutral): form, a x2 multiply, and
shine, a lit highlight per material. A model that is not there is left
out, so its look keeps the neutral layers.
"""
import os, sys, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import clamp, write_tga
from md3bake import Files, bake, sample


def tga_bytes(w, h, px, scratch):
    write_tga(scratch, w, h, px, False)
    with open(scratch, "rb") as f:
        data = f.read()
    os.remove(scratch)
    return data


def build(folder, out):
    """Bake every FORM sheet into the pack out."""
    import looks
    from skins import form_images
    files, scratch, made = Files(folder), out + ".tga", {}
    for module, look in looks.ALL():
        for name, f in getattr(module, "FORM", {}).items():
            maps = bake(files, *f["model"])
            if maps is None:
                print("bake-form: no %s in %s; %s keeps its flat look" % (f["model"][0], folder, look.skin))
                continue
            lit, opn = maps
            size, label, shine = f.get("size", 256), f.get("label", lambda u, v: ""), f.get("shine", {})
            fp, sp = [], []
            for y in range(size):
                for x in range(size):
                    u, v = (x + 0.5) / size, (y + 0.5) / size
                    l, o = sample(lit, u, v), f["open"][0] + f["open"][1] * sample(opn, u, v)
                    fp.append([clamp((f["light"][0] + f["light"][1] * l) * o * 0.5)] * 3 + [1])
                    sp.append([clamp(shine.get(label(u, v), 0.0) * l ** 8 * o)] * 3 + [1])
            form_img, shine_img = form_images(look, name)
            made[form_img] = tga_bytes(size, size, fp, scratch)
            made[shine_img] = tga_bytes(size, size, sp, scratch)
    with zipfile.ZipFile(out + ".part", "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in sorted(made.items()):
            z.writestr(name, data)
    os.replace(out + ".part", out)


if __name__ == "__main__":
    build(sys.argv[1], sys.argv[2])
