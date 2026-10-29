# Logo

The first working logo for tremor-invictus, superseded by the approved
branding (an art pack kept outside the repository; see "UI branding" in the
top README). It stays only as the drawn placeholder menu logo for builds
without that pack; nothing uses it as an app icon any more.

- **Emblem:** a seismograph trace inside a worn metal ring. The trace stays
  inside the ring.
- **Wordmark:** TREMOR in Cinzel Black, cut by a fracture shaped like the
  seismograph trace, over a wide-spaced red INVICTVS in Cinzel Bold.

| File | Use |
|---|---|
| `tremor-invictus-horizontal-on-dark.svg` / `-on-light.svg` | Default lockup: headers, splash, store banners |
| `tremor-invictus-stacked-on-dark.svg` / `-on-light.svg` | Square-ish spaces: menu title, posters |
| `tremor-invictus-emblem-on-dark.svg` / `-on-light.svg` | App icon, favicon, small marks |

"On dark" files have light metal and are meant for dark backgrounds; "on
light" files are for light backgrounds. All backgrounds are transparent.

Colours: red `#d3191f` on dark, `#a5131a` on light.

All lettering is outlined, so the SVGs need no fonts. To regenerate after a
change, get Cinzel (SIL Open Font License, Google Fonts) as `Cinzel-700.ttf`
and `Cinzel-900.ttf` in a folder outside the repo, then run:

    pip install fonttools
    python3 misc/logo/make-logo.py /path/to/fonts misc/logo
