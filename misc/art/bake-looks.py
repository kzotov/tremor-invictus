"""The champions' looks (skins.py, looks/), painted on the player's own
machine from the player models in the player's own Quake III data, into a
pack that stays in their home folder: none of it is in the repository, in
tremor-assets.pk3 or in any package.

  python3 -I misc/art/bake-looks.py BASEQ3 WORK_DIR OUT.pk3

BASEQ3 is the folder with the player's pak0.pk3. WORK_DIR keeps the
painted images between runs (a look is painted again only when its module,
the painting code or the player's pak0.pk3 changed). A look whose model is
not there is left out, and the champion wears the model's own skin.
"""
import os, sys, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import skins
from geo import Context


def main(folder, work, out):
    src = os.path.join(work, "src")
    os.makedirs(src, exist_ok=True)
    pak = os.path.join(folder, "pak0.pk3")
    signature = ("%s %d %d" % (pak, os.path.getsize(pak), int(os.path.getmtime(pak)))).encode() if os.path.isfile(pak) else b""
    skins.build(src, Context(folder), signature)
    with zipfile.ZipFile(out + ".part", "w", zipfile.ZIP_DEFLATED) as z:
        for root, _, files in os.walk(src):
            for name in sorted(files):
                full = os.path.join(root, name)
                z.write(full, os.path.relpath(full, src))
    os.replace(out + ".part", out)


if __name__ == "__main__":
    main(*sys.argv[1:4])
