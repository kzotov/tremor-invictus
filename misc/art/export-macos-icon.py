#!/usr/bin/env python3
"""Export the approved macOS Dock icon from its master into the branding pack.

  python3 misc/art/export-macos-icon.py PACK [MASTER]

MASTER (default PACK/masters/macos-dock-icon-approved.png) is the finished square
RGBA artwork: the charcoal rounded tile with the emblem, transparent outside the
tile. It is exported as it is, at its own placement and margins, with no tile,
mask, inset or shadow added, into
  PACK/icons/macos-dock/TremorInvictus.iconset/   icon_16x16 ... icon_512x512@2x
  PACK/icons/macos-dock/TremorInvictus.icns       (iconutil; all but the 16 and
                                                  32 px 1x images, see below)
which import-brand-art.py stages as the macOS bundle icon.

One correction: the master's tile is drawn at 251-254 alpha, not 255. Recent macOS
reads a tile that is not fully opaque as a non-conforming legacy icon and sets it on
a grey plate in the Dock and Finder; alpha of 240 and above becomes 255, in the
master and again in each size (the picture is unchanged, the edges keep their soft
alpha).

Needs Pillow and macOS's iconutil. The art stays outside the repository.
"""
import os, shutil, subprocess, sys, tempfile
from PIL import Image

SIZES = (16, 32, 128, 256, 512)


def opaque(image):
    image.putalpha(image.getchannel("A").point(lambda a: 255 if a >= 240 else a))


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    pack = sys.argv[1]
    master = sys.argv[2] if len(sys.argv) == 3 else os.path.join(pack, "masters", "macos-dock-icon-approved.png")
    src = Image.open(master)
    if src.mode != "RGBA" or src.width != src.height or src.width < 1024:
        sys.exit("export-macos-icon: %s is not a square RGBA picture of 1024 px or more" % master)
    src = src.copy()
    opaque(src)
    out = os.path.join(pack, "icons", "macos-dock")
    iconset = os.path.join(out, "TremorInvictus.iconset")
    os.makedirs(iconset, exist_ok=True)
    for size in SIZES:
        for scale, suffix in ((1, ""), (2, "@2x")):
            # Pillow scales RGBA premultiplied: no dark fringe on the soft edges;
            # its filter dips the alpha again inside the tile at small sizes
            image = src.resize((size * scale, size * scale), Image.LANCZOS)
            opaque(image)
            image.save(os.path.join(iconset, "icon_%dx%d%s.png" % (size, size, suffix)))
    # the .icns leaves out the 16 and 32 px 1x images: iconutil stores those two
    # as ARGB, which macOS 26 still sets on the grey plate in Finder's small
    # sizes; without them it scales down the 32 and 64 px PNGs, plate-free
    with tempfile.TemporaryDirectory() as work:
        icns_set = os.path.join(work, "TremorInvictus.iconset")
        shutil.copytree(iconset, icns_set)
        for name in ("icon_16x16.png", "icon_32x32.png"):
            os.remove(os.path.join(icns_set, name))
        subprocess.check_call(["iconutil", "-c", "icns", icns_set, "-o", os.path.join(out, "TremorInvictus.icns")])
    print(os.path.join(out, "TremorInvictus.icns"))


if __name__ == "__main__":
    main()
