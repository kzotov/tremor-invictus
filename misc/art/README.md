# Generated art

The repository keeps no images: the pre-commit guard refuses them. These
scripts draw the game's own pictures from shapes and noise instead, and the
build runs them at configure time (`cmake/identity.cmake`) into
`build-*/generated/gfx/tremor/`, which the run scripts link into the game's
home folder. Plain Python 3, no packages.

| Script | Draws |
|---|---|
| `make-splash.py` | `splash/back` behind the connect and loading screens, `splash/nomap` for maps without a levelshot |
| `make-medals.py` | `medals/<key>`: every match medal, the classic awards and the end-of-match ones |
| `make-abilities.py` | `abilities/<name>` HUD icons and the `fx/` effect textures (shaders in `assets/baseq3/scripts/tremor-fx.shader`) |
| `portraits.py` | `gfx/champions/<name>` portraits for the champions that have none elsewhere, packed by `make-models.py` |
| `tremorart.py` | shared: the logo's palette, shapes, noise, glow, TGA output |

The pillar and podium textures come from `misc/textures/make-pillar-textures.py`.

The splash pictures, the console background and the menu logo drawn here
are placeholders: `import-brand-art.py` copies the approved branding pack
(kept outside the repository, see "UI branding" in the top README) over them
after every configure and build, and stages the app icons.

To change a picture, change its script; the next configure redraws
everything here. To draw by hand into a folder:

    python3 misc/art/make-medals.py /tmp/out             # all medals
    python3 misc/art/make-medals.py /tmp/out railgod      # just one
