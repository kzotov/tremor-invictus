"""Galena: lucy's armour under uriel's hood, the body painted from scratch
as a priestess's: ivory plate with gold trim and engraved lines, a cream
breastplate with a gold cross, layered pauldrons banded in gold, a gold
belt, a cream tabard bordered in gold over near-black robes, and ivory
boots; lit from the model's own form. The hood is near-black with a gold
edge; under it a pale face framed by long black hair, with a red mark on
the brow. Team versions dye the robes and the hood in the team's colour."""
import math, re
from skins import Sheet, Look, LabelMap, region, inside, seg_dist
from tremorart import Fbm, Noise, clamp, smooth


def levels(rows):
    return [[ord(c) - 97 for c, n in re.findall(r"([a-p])(\d*)", row) for _ in range(int(n or 1))] for row in rows]


def sample(grid, u, v):
    """0..1, smoothly sampled from a grid of levels."""
    n = len(grid)
    x, y = u * n - 0.5, v * n - 0.5
    x0, y0 = max(0, min(n - 1, int(math.floor(x)))), max(0, min(n - 1, int(math.floor(y))))
    x1, y1 = min(n - 1, x0 + 1), min(n - 1, y0 + 1)
    fx, fy = clamp(x - x0), clamp(y - y0)
    top = grid[y0][x0] + (grid[y0][x1] - grid[y0][x0]) * fx
    bottom = grid[y1][x0] + (grid[y1][x1] - grid[y1][x0]) * fx
    return (top + (bottom - top) * fy + 0.5) / 16


# ---------------------------------------------------------------- the body sheet (lucy's angel.tga)

# what each texel is, 128 x 128, run-length coded rows: w ivory plate,
# c breastplate, p pauldrons, g gauntlets, b belt, t tabard, d robes,
# o boots, k nothing
MAP = LabelMap((
    "w3k4w5gw13g2w27c12o35w26",
    "w3k4w46c15o29w31",
    "w3k4w45c18o26w32",
    "w3k4w43c22o23w33",
    "w3k4w42c23k23w33",
    "w3k4w39cwc25k22w33",
    "w3k4w35c31k22w33",
    "w3k4w35c32k21w33",
    "w3k4w34c33k21w33",
    "w3k4w33c34k21w33",
    "w3k4w33c34k22w32",
    "w3k4w32c35k22w32",
    "w3k4w32c35k23w31",
    "w3k4w31c36k23w31",
    "w3k4w31c36k23c11w20",
    "w3k4w31c36k23c11w20",
    "w3k3w31c37k22c13w19",
    "w3k2w31c38k22c13w19",
    "w3kw31c39k21c14w19",
    "w34c40k21c14w19",
    "w33c41k21c14w19",
    "w32c42k20c15w19",
    "w31c43k20c15w19",
    "w31c43k20c15w19",
    "w30c44k18c17w19",
    "w30c44k16c20w18",
    "w29c45k15c21w18",
    "w29c45k13c23w18",
    "w29c45k11c25w18",
    "w29c45k10c26w18",
    "w28c46k10c26w18",
    "w28c46k10c26w18",
    "w27c46k11c26w18",
    "w27c46k11c11d14cw18",
    "w27c46k11d44",
    "w26c47b11d44",
    "w26c46b12d44",
    "w26c45b14d43",
    "w26c45b17d39w",
    "w26c44b21d35w2",
    "w4dw21c42b30d19b8dw2",
    "w3d5w18c42b58w2",
    "w2d10w14c42b58w2",
    "w2d16w9c41b58w2",
    "wd23w3c41b59w",
    "wd28c39b59w",
    "wd36c31b59w",
    "d44c23b60d",
    "d48c4dc13b61d",
    "d50c16b62",
    "d46c20b62",
    "bd42c23b62",
    "b2d24bd27c12b23d3b36",
    "b2d21b5d6bd26c5b4d11b3td9b34",
    "b3d20b12d27c3d16t3d11b33",
    "d22b3d9bd28c2d17t3d16b6d5b11d5",
    "d21b3d10bd93",
    "d3k18d13td93",
    "d3k18d12t2d93",
    "d3k18d10t4d26kt2d64",
    "d3k18d7t7d24k3t3d4t2d57",
    "d3p13k5d36k4t5d2t3d57",
    "d2p19d34k5dt10d57",
    "dp21d31k7d2t10d56",
    "dp26d24k3b6d3t9d56",
    "dp34d16k3b7d3t8d56",
    "dp37d12k4b8d3t8d55",
    "dp42d6k5b9d2t8d55",
    "dp43d5k5b10t9d55",
    "dp44k9b10td3t5d55",
    "dp44k9b11d3t4d56",
    "dp44k9b12d2t5d55",
    "dp10dp33k9b12d2t5d55",
    "dp9d4p32k8b13dt5d55",
    "p9d7p30k8b13dt5d51t4",
    "p8d10p28k8b13t6d50t5",
    "p6d14p26k8b14t6d49t5",
    "p5d17p24dk7b14t6d48t6",
    "pd22p22d2k7b14t6d47t7",
    "d25p13d9k7b15t5d47t7",
    "d26p3dp5d12k7b15t5d46t8",
    "d30p3d14k8b15t5d44t9",
    "d31pd15k8b15t5d44t9",
    "d47k8b11t9d43t9d",
    "d47k8b2t18d42t10d",
    "d47k8bt15d3td17w3d22t10d",
    "d47k9t15d3t2d9w20d12t9kd",
    "d47k9t15d3w34d8t10d2",
    "d47k9t15d2w38d5t7w3d2",
    "d47k9t16w54d2",
    "d47k9t16w54d2",
    "d47k9t16w54d2",
    "d11gd35k10t16w53d2",
    "d11g2d33k11t15w54d2",
    "d11gd34k11t15w55d",
    "d11gd33k12t15w55d",
    "d10g5d30k12t15w55d",
    "d10g8d26k13t15w55d",
    "d10g9d25k14t14w55d",
    "d9g11d23k15t13w56d",
    "d9g14d20k15t13w56d",
    "d9g14d20k15t12w57d",
    "d8g16d19k15t11w58d",
    "d8g16d19k16t9w59d",
    "d8g17d18k25w59d",
    "d7g18d18k24w60d",
    "d8g18d17k24w61",
    "d8g18d17k23w62",
    "d8g19d16k22w63",
    "d8g19d16k21w64",
    "wd7g20d15k21w64",
    "wd8g19d15k14o7w64",
    "wd8g19d15k13o9w63",
    "wd8g19d15k13o10w27ow22o3w9",
    "wd8g20d14k12o11w14o26w8o11w3",
    "wkd8g18dg3d11k11o53w3o16w2",
    "wkd8g19d14k11o73w",
    "wk2d8g18d14k11o74",
    "k3d9g18d12k12o74",
    "k4d8g18d12k12o73k",
    "k4d9g17d11k13o72k2",
    "k4d9g19d9k13o71k3",
    "k4d9g21d7k13o70k4",
    "k4d9g20d8k13o69k5",
    "k4d9g19d9w13o69k5",
    "w3kd7g22d6w16o67w6",
    "w3k2d5g24d3w18c4o60w9",
    "w3k3dw3g24w21c8o41wo11w12",
))
# how much each texel faces a light from above and in front, 16 levels
LIT = levels((
    "n15m4l3k3lmn4m2lmop5o4n4m5l2m3n5m2khg3h2ihfdc5d2ef2g3h2i2gdb3dgjkmno5n2lm4lk5lmn3",
    "n15m4l3k4lmn2m2l3op6o4n3m8n8mlkhg2h4fdc5d2ef2g3h2i2gdc2dfhjkmno5n2m5l3k2l2m2n2",
    "n15m3l3k3jk2mn2m2l3nop6o4n4m5n2o4n3m2ljh6fdc5d2ef2g3h2i2he3fghjklmno3n4m6l5m3n",
    "m5n10m3l2k3j3km4l2klno2p5o5n8o7n2m2l2kjih3fdc5d2ef2g3h2i2hf4ghijlmn9m14",
    "m5n9m3l2k3j4klm2l3k2lno3p4o5n6o2p3o3n2m3l3j2i2gec5d2ef2g3h2ihgf5ghjkm2n3m2n3m14",
    "l3m2n9m2l3k3j4k2l4k3lno14n2o3p4o2n2m3l5k3igec4d2ef2g3h3gf6ghiklm19l4",
    "l4mn8m2l3k3j5k2l4k4mno16p6o2n2m3l8kigedcd3ef2g3h3gf7ghjkl5m10l8",
    "l5mn6m3l2k3j6k2l4k4lmno15p6o2n2m3l8kihge5f2g3h4f8ghik5l2m5l12",
    "l5m2n4m3l2k3j6k3l5k4mn3o12p7o2n2m3l8kjihg4h10f9gij4k2l18",
    "k4lm8l2k4j6k2l9km2n4o10p7o2n2m3l3k6ji2h14gf8ghi2j3k2l10klk5",
    "k5lm5l5k4j4k3l8mlm3n5o7p8o2n2m3l2k8i2h15f9ghi3j3kl4k12",
    "k6l12k3j2k3l9m4n6o6p8o2n3m3lk9ji2h14gf8gh3i2j3k16",
    "k13l7k6l10m3n6o6p8o2n3m2l2k9ji2h15f8g2h3i3j2k15",
    "j5k9l8kl13m4n5o6p8o2n3m2l2k9ji2h16g8h3i4j2k10j5",
    "j5k10l18m7n5o5p9o2n3m2l2k9ji2h13i2j2k2j12k11j6",
    "j5k11l13m10n5o6p8o3n3m2l2k9ji2h12i2kl7k18j8",
    "j5k12l8m15n4o6p8o2n4m2l2k9ji2h12ijkl8k16j9",
    "j6k12l6m16n4o6p7o3n4m2l3k9i2h11i2kl9k15j10",
    "j12k5l7m16n4o5p8o3n4m3l2k9i2h11i2kl9k5j20",
    "j4i4j4k4l4m14l3m3n4o5p7o4n4m3l2k9i2h11ijl9k3j23",
    "j3i7j2k3l4m11l7m4n3o5p8o3n4m3l2k9i2h10i2klm6l2k2j24",
    "ji2h4i3j3k2l3m12l7m4n4o4p8o3n4m3l2k9i2h10ijlm7l2k2j24",
    "i3g2h3i3j2k2l3m11l8m4n5o4p7o3n4m3l2k9i2h9i2klm7l2kj23i2",
    "i2hg2h4i2j2k2l3m11l10m3n4o4p8o2n4m3l2k9i2h8i2jklm6l3kj21i4",
    "ihg4h3i2j2k2l4m9l4k3l5m3n4o3p8on5m3l3k8i2h7i2jkl2m6l2k2j20i5",
    "igf2g3h2i2j2k2l4m8l4k7l3m3n3o4p7on5m3l3k8i2h6i2jk2l3m5l2k2j19i6",
    "hf4g2h3i2j2k2l3m7l4k10l2m3n2o4p7on5m3l3k8i2h5i2jk3l4m4l2k2j12i13",
    "fef3g3h2i2j2k2l13k12l2m2n3o4p6onm2n2m3l9kji2h4i2jk4l4m3l3kj9i17",
    "e3f3g3h2i2j2k2l12k13l2m2n3o5p3o2nm3nm3l9kjih4i2jk5l9k2j2i24",
    "d2e3f3gh2i2j2k2l10k16lm3n3o9nm3nm3l9ki3h2i2j2k6l8k2j2i23h",
    "d3e3f2g2h2i2j2k2l4k16j4kl2m3n3o8nm7l9kji5j2k7l4k2lk3ji23hg",
    "c2d2e3f2gh2i2j3k13j13kl2m3n3o7nm7l10k2ji2j4k15j2i23hg",
    "c3d2e3f2gh2i2j2k10j13i2jk2l2m3n6o3nm8l10k3j7k5j8i24h2g",
    "c4d3ef2g2h2i2j19i8jk2l2m3n9m8l10k3j18i11h17f",
    "c5d2e2f2gh3i2j13i11hi2jk2l2m4n7m8l5m2l3k3j18i6h5g2f4e4f5gfe",
    "cb2c3d2ef2g2h2i12j2i3h11i2jk2l2m5n4m3lm5l5m2l3k4j3k2j12i4h3gf4ecb4cd6e3d",
    "b4c2d3ef2g2h2i10j2i2h9g3h2i2jk2l4m3n2m3l2m5l4m3l3k3j3km2lkj9i3hg2fecb5a6b3c3d3c",
    "b5c2d2e2f2g2h2i12h2g12h2ij2k3l3m7l3m4l2m5l3k3j3ko3mlkj6ihgfdcb3a15b4c3",
    "b6c2d2ef2g3h3i9hg4f8g3h2ij2k3l3m6l3m4lm6l3k3j3kop2o2mlkj3igdcb2a20b4cb",
    "b7cd2e2f2g3h3i7hg2f5e3f4g3h2ij3k3l3m3l5m5n2m3l3k3j3kop4o2mlk2jfdc12b3c3d6c3b2",
    "b2a3b2c2d2e2f3g2h4i3h2g2f3e8f3g2h3ij4k3l9m3n4m3l2k3j4kno2p5o3nki2h16jk4j2igdcb",
    "ba5b2cd2e3f3g3h6g2f2e11f3g2h2i3j4k2l9m2n3m3l3k2j4ijk2mop8on8m17lkgcb",
    "ba5b2c2d2e3f2g4h2g4f2e4d3e7f2g2h3i3j3kl4kl4m7l3k2j2i5jknop15o2no6n5m4l3idb",
    "a6b3c2d3e2f3g4f5e3d3e9f3g3h3i3j2kl3k2l4m4l4k4j5kmnop15onl2mn3o4n3m3l3kidb",
    "a8b2c2d3e2f6e8d6e7f4g3h3i2jk2lk4l5ml5k2lm5n2o2p13o2nlkj4klno2n3m3l3k2idb",
    "a9b2c2d3e9d2e4d12e4f3g3h3ijk6l7m7n3o6p10o2nlkj2i6jmn4m3l3k3ifc",
    "a9b3c3d2e4d6e5d13e4f2g4hi2j2k4l8m6n4o6p5o2n2lkj2i6h3ijmn2m3l3k3jifc",
    "ba9b3c3d10e3f2e2d4c6d3e4f2g2h2ij3k4l10m6n2o10nmk2ji5h9ijm4l3k4jifc",
    "b3a7b4c3d9e3f4e2d3c6d4e2fg2i3j4k12l6m3n2o7nmkji4h14ikm2l3k4j2ihe",
    "b5a5b4c6d2c2d2e3f7e2d2c5de3fhi3j6k7j2k9l2mn4o3nmkjih11g6h3jl4k4j3i2h",
    "dcb9a3b3c8de3f3g5f2ed3e2fgh3i3j8k4j6i4h2ijlm2n6mki2h8g12hikl2k4j3i4",
    "gec2b6a6b6c2def6g8f3g2h3i7j7k2j5i3h3g3iklm2n3mlki3h2i5hg12h2jk5j3i3h2",
    "g2ec6b2a7b4cdfgh2gf2g2h3gh3gf3g9h7ij2kj2i5h3g4hiklm3lkji3h2ilmnmlji2h10ijk5j3i2h4",
    "g2fdc5b2a8b2cdfghijih3ijkih4gdc17dfij3i5h3g5h2ilm2lji5h2ilo4nm6l4k6j5i2h4g",
    "g2fdc5b2a8b2cfgh2ijk2lm2nmjh4gcba16beij2i5h2g4h5ikl2ki6h3jno8n4m2l3k2j3i4h2g4",
    "f3edc4b2a8b2cfg2fghiklmn2mjh4gdba17dhji2h6g2h7ijklkji4h4imo9n3m2l2k2j3i3h3g3f2",
    "f2e2d2c3b2a8b2cd5eghjklmli4hgdb2a16chjih14i4j3i5h3gimo8n3m2l2k2j3i2h4g2f4",
    "e6d2c2b2a6b9cdeghj3i4hecb3a15chih14i13h2g2imo7n3m2l2k2j2i3h3g3f2e3",
    "d2e2fihgfe2dc7ba8bcdfgh4i2hecb3a15cg8h6i6j2i7h2g2imo6n3m3l2k2ji3h3g2f2e3d2",
    "d3fjmnml3kjh5gdba9bcefgh2i2hec2b3a13bdfgf4g2h4i7j4i5h3g2hmo5n3m3l2k2j2i2h3g2f2e3d3",
    "cdeimo5n3m3n2lieca7b2cdefh2i2hedc2ba12bcdf7g3h2i6j3kj2i5h2g3hmo4n3m3l2k2j2ih3g2f2e3d4c",
    "cdglop3o9ponlheca3bcde2f2gh2i2hfdc2ba11bdgjhf7g2h2i5j3k3j3i3h2g3hmo2n4m3l2k2ji2hg3f2e2d5c3",
    "cejnop3o8p4onkhec3de2f4g3hgfdb2a11cejlmjgfe3f3ghi4j3k5j3i2h3g3hmn5m3l2kj2ihg2f3e3d2c7",
    "cflop4o8p6onlihgf5ed7b2a10bdgkmn2mjfe4f2ghi2j4k3l2kj3i2h2g3fhmn5m3l2kjihgf2e4d2c4bc2b2",
    "ejnop4o8p8on2mk2j2igedc3d2ba10behln5lgd2e2f2g2ij4k3l4kji3h3g2fhmn5m3lk2jihgf2e4d2c3b6",
    "flo2p4o8p8o2p2o2n2mlife4dba9cejmn6mgd3e2f2gi2j4k2l4kji3h3gf2hln5m2l2kj2ihgfe4d4c2b6",
    "flo2p3o3n4o5p5o2p5o2nlkl2kgeca8dhkmn7miecde3fghi4j2k3l2kj2i2h2g2f2gln4m3l2kjihg2fe3d5c3b5",
    "hmo3po3n11o7p4o2nm2n2mifecba5chln8onlhd3e3fgij2ij4k4j2i2h2g2f2gln4m2l3kjihg2fe2d6c4b4",
    "jmo7nm12n5o5nm3nomigf2dba2bcgkn9o2nkfd2c3egij8k3j2i2h2gf3gln4m2l2k2jihg2fe2d5c6b3",
    "jno6n2m6l7m4n5ml2mnonjih2gfedegimn10onmgdb4dfij10k2ji2h2gf2egln3m3l2kj2ihg2fe2d4c6b4",
    "jmo6nml6k7l4m5l3mno2mlk3j3i2jmn10o2nkfba2bcfij13i2hg2fe2gln3m2l2k2ji2hg2fe2d4c5b5",
    "kno5n2mlk5j8k4l5klmno2nkih2g4hjmn11o2njda2bcegj13i2hgf2e2gln2m3l2k2jih2gf2e2d3c5b4a2",
    "mno4n3mlkj5i8j3k6lmno2mhgf6giln11o2nleba2bdgj12i2h2gf2e2glm4l2k2j2ih2gf2e2d3c5b3a3",
    "n2o3n3m2kj2i6h2g2h3ij2k3jklmno2mig7hiln12onmgba2bdfik3j8i2hg2fe3flm4l2kji3h2gf2e2d2c5b3a4",
    "n8mlkji3h5g2f2g2hij5klmno2nml7kjlm7n5o2mhca2bcfik5j6i2hg2fe3fkm3l2kjhghih2gf2ed3c5b2a5",
    "n7m2lkjih5g3f4ghij5klmno2n2l3k5jlm8n5onkeba2ceik9j2ih2gf2e3fkm3l2khe2hih2gf2ed3c4b2a6",
    "n6m2lkjih3g5f2ef2ghi5jklmno2nmjhg2f3ghkm8n5on2jca2bdgjk8j2ih2gf2e3fkm3lkied2hih2gf2ed3c3b3a6",
    "n5m3lkjih2g3f3e4fghi5jklmno2nlhgf6gjlm8n5onlda2bdfjk7j2i2h2gfe4fkm2lkifdcdhih2gf2ed2c4b2a7",
    "m7l2kjih2gf4e5f2gh3i2jklmno2nmk2j8lm8n5onlfbabcfjklk5j2i2hg2fe4fkm2lifdc2dgh2gf2e2d2c3b2a5b3",
    "m7lkji2hg2f2e8fgh3i2jklmno2nmlk2j7lm9n6mhcabcfjkl4k2j2i2hg2fe4fkmlifdc3eghg2f2e2d2c2b3a3b4d",
    "m6l2kji2hg2fe9fg3hij2klmno2nmligfe4fhkm9n5onkeb2ceikl4k2j2ih2gf2e4fkljfdc3deghg2fe2d3c2b2a3b4cf",
    "m5l2k2jih2gf2e3d2e4fg3hij2klmnon2mkgfe6gklm9n6mhcb2dgkl4k2ji2h2gf2e4fj2fd2c2d2eg3f2e2d2c2b3a2bc4dg",
    "l7kj2ih2gfe3d4e3f2g2hij2klmnon2mlj2i6j2l2m8n5onkecbdfjl3k2j2i2h2gf2e4fhfd7eg3f2e2d2c2b3ab2c4ei",
    "l6k2ji2h2gf2e3d4e3f2ghijk2lmn4mlk9jl2m9n6lgdc2fjl3k2j2i2hg2f2e7d7eg3fe3dc3b5cd4fj",
    "l5k3j2i2hgf2e3d4e3f2gij2k2lmn4ml2k8jl8m3n6mife2fjl3k2j2i2hg2f2e5d8e2g2f2e2d2c3b4cd5fj",
    "l6k3j2i2gf2e3d4e3fghijk3lmn4mlk9jkl8m3n6lkjigjl3kj2i2h2g2f2e5d7e2fg2f2ed2c4b3cd5ehk",
    "l7k3j2ihgfe4d3e2d2fhijk2l2mn4mlk9jkl9m2n6ml2jgiklk2j2i2h2g2f2e6d4e4fgf3ed2c4bc2d6fik",
    "m4l4k3jihgfe4d5c2fhjkl4mn4mlk9jkl9m3n6mlkijl3k2jih3g2f2e13f2gf2e2d2c7d2e4fjl",
    "m7l2k2jihgfe4d3cbabehjkl4mn4mlk8jijkl8m3n6mk2jkl2k2j2ihg2f3e12f4gf2ed3c6d2ef4hkm",
    "m8l2k2jigf2e3d2cba2behjlm5n3m2lk8i3kl9m2n6mig2iklk2jih2gf7e8f4gf2e2d3c5d2efhi2jklm",
    "m4n2m3l2kjihgfe4db2a3dgklm5n3m2l7kji3k2l8m3n5mjh2ikl2kj2ihg5f4e3f7g3f2ed3c4d3efij3km2",
    "n8m2lkjhgf2ef2gfdcb3cgkm2n2m2n3m2l7kji3k9l2m2n6lk2l4k2j2ih3g3f3e4f5g5f2ed8e2fgh3ilm",
    "n9mlkife4fgh4f2edfkmn8m2l7ki4k9l2m3n5m2l6k2ji3h2g3f2e4f3g5h2gfe2d7e2f2g3hln",
    "n4o3n2mlkgd3e2fghi2jk2jihkmn8m7l2jihi2jk9l2m2n5m4l4k2j2i2h3g2f3e4f2g4h5fe2d6e2f4ghln",
    "n3o5nmljec3defgh2ijln2mlmno4n4m8ki2h2i2jk8l2m3n4m4l4k2j2i2h3g3f2e4f2g2h5i2hge2d4e3f4ghln",
    "n2o6nmlidb2cdefg2hijlno10n3m7ljih3i2jk9l2m2n4m5l3k2j3ih3g3f3e3f3gh4i3jige8f2g2himn",
    "o4p2o2nmlida2bcdefghijkmop4o6n7m2jih4i2jk9l2m3n2m5l4k3j2ih3g3f3e2fhgfg2h2i3j3ife5f3gijklno",
    "o4p3onmlida3bcdfghijklnop3o3po9nmjih4i2j2k8l3m10l4k2j2i2h2g3f3e2fhf3gh2i3j4hg7hikl2mno",
    "o3p4onmljdba4ceghijklmop3o3p2o9njih4i2j9k2l2m4l2m5l3k2j2i2h3g2f4e2f4g2h2i3j7k5l2m3no",
    "p7on2ljeb2a4cefhjk2mnp3o3p10onjih5i2j8k2l2m3lj2lm4l4k2ji2h3g3f3e5f2g2h2i3j2k3l5m6no",
    "p7on2ljecba5bcdhkl2mopo4p11nkih6i2j7k2l2m2ljghkm5l3k2j2ih3g3f3e6f2g2h2i2j3k2l4m7no",
    "p7on2mjecba7cgjklmno5p11omjih6i2j5k3l2m2khf2hlm4l3k2j2ih3g3f3e2d2e2f2g2h3i2j3kl10mno",
    "p7o2nmjfcb2a6bdiklmnono3p12njih7i2j3k4l3kifefgkm4l4kj2i2h2g3f3e2d3e2f2g3h2i2j2k2l10mo",
    "p7o2nmjfcb2a7cgjlm2n2o3p12ojih8i2j2k4l2kige3fhlm4l3k2ji2h2g3f3e2d4e2f2g2h2i3j2kl10mo",
    "p7o2nmkfdb2a7beiklmn2o3p12ojih9i2j2k5ife4fgkm4l3k2ji2h2g3f3e2d5e2fg3h2i3jk2l9mo",
    "p7o2nmkgdcba7bdgjl2mn2o2p12ojih10i2j2k2jhfe6film4l3kj2ih2g3f2e3d5e2fg4h2i2jk3l8mn",
    "p8onmkhecba7bcfiklmn2o2p12ojih11i2j3hfe7fgkm4l3kj2ih2g3f2e2d7e2g2fg2h2i2jk3l8m",
    "p8o2nliecb2a6bcehjklmno2p12ojih11i2jihfe2d3e4film4l2k2jih2g4fe2d3cd4ef4g2hi2jk3l9",
    "op8onlifdcba6bcdfijlmnop13ojih10i5ged7e2fhkm4l3kji2hg3f2e2d2c3d3ef5gh2ij2k3l8",
    "mop7onlifdcba6bcdfhjkmnop13ojih9i6ged7e2fgilm4l2kji2h2g2f2e2d2c5def3e2fgh2ijk4l7",
    "mop7onlifecb2a5bcdegiklno2p12ojih8i7hed7e3fhkm4l2kj2ih2g2f2e2d2c5d2f3e3fghij2k4l6",
    "lnp7onligedcba4b2cdegijlmnop12ojih7i8hedcd5e3fgjlm4lk2jih2g2f2e2d2c6df3e4gh2ij2k9",
    "lnop7nligfdcb2a3b2cdfghjkmnop12njih7i8hedc2d4e2f2hjlm4l2kji2h2gf2ed2c7df3e2defghij3k8",
    "lmop7nligfec2ba3b2cdfghiklnop12mjih9i6hfdc2d4e2fgh2ikm5lkj2ihgf2ed2c7df2e3d2efg2h2i5jk3",
    "kmop7nlihfedcba3b2cdfghijlmo2p10okih11i5hfdc3d3e2fg2fgijlmn2m2lkjihgfed2c7defe2d13egij2",
    "kmop7omjhfedcb6cdefhijlmo2p10mjih12i3h2gedc4d2ef2ef2gijlmn3mlk2igfed2c3b2c3de2d2c4b2cb6dfhi",
    "knp8omkigedcb6cdefhjkmnop10okih5gf4g8fedc5dfede2fghijlmn3ml2jhfe2dc3b3c3e2d2c2b6a4bcefg",
    "mop8onljgedc2b5c2dfgjlmnop10njih4fdc6de7dcb3cedcd2ef2hijklmn3mlkigfed2c2b5cd3c2b6a6bcef",
    "op10nmkhfedc2b4c2degilmnop10njih3fcba6b2c8b4c4de2fgijlm3n3mljhge2dc2b6c4b5a8bcde",
    "p11omkife2dc3b2c2degilno3p9nki2hgdba12b3c4b5cd2efgikn2m2no2nmkigfedcb2a4b2c2b4a4b4c3deg",
    "o8p3onljgfed2c6efhjln2o2p9omkjihdba14b2cd2cb4c2de2giko2n3o3mkjhfedcba6bcb4a4cef5g2h2",
    "no8p2onmkhgfed2c4dfhjkmn2o2p8o2nm2lkhecb7a7bcde2cb3c2d2efimo3n2o3mkihfedba8b3a5bdfgh4ijkj",
    "lno6p4onligfe2d4efhjklmn2o2p6o4n3m2kjhedcb8a2b2cdfgfdb3c2defjno2n4o2mjhgedcba8b3a4bcefg2hijlm3",
    "mlno4p5onljhgfe3deghjklmn2o2p7o4n4m3ljgedc5b5c2dfghgdcbc2degknonm4n2migedcba9b2a4b2cefghijlmn3",
    "nmlno2p7omkihgf2efghjklmn2o2p8o4n4m2nmlkiged2c8defhi2hdcbcdehlm2l2k2l2mlhfdcba2bc2b2a3b2a4b2cdfhj2klmn3",
    "n2mlnop7omlj2ih2ghijklmn2op10o4n4m2n2ml2kjhgfed6e2fhijiecbcdfil2k2jij2k2jgecb2a2b2cd3cb3a4b2dfijk3lmn3",
    "n3m2o3p5onml4k2j2klmn2o6p6o4n4m2n2ml6kihfe5fghijifc3dgi4h4i2jigecba4bdgj2gecba4bdgik5lmn3",
    "n9o6n6m3lm2no5n2op5o4n4m2n2ml3m4l2jhe3f2ghi3fc4e2f4g3h2i2hfcba4dhkmnljgdba2beik7lmn3",
))
# how open each texel is to the sky
OPEN = levels((
    "p13op13op100",
    "p128",
    "p128",
    "p128",
    "p109o2p17",
    "p62o2p43ono2p17",
    "p61on2op41om2nop17",
    "p60onm2np41omno2p17",
    "p59onljkmop40n2op19",
    "p58onlihilnp40o2p20",
    "p57o2mjgfhknop61",
    "p54on2omkhfegjmop61",
    "p53omjkmlifd2fhlnp61",
    "p52omjg2jigedcdgknp61",
    "p51onlife2fec3dfjmop60",
    "p51omkhfdcdc2b2cfjmop60",
    "p50o2mkhfdc2b2abcfimop26o8p26",
    "p50onmkifdb3a3cfimop25o9p26",
    "p50onmkigecba4cfilop25o9p26",
    "p6op43onmljhfdba4cfilnp25n8op26",
    "p5o3p42o2mlkigedba3cfilnp24n3m2n4op26",
    "p3o6p41o2nmljhgeca3cfilnp23onm7nop26",
    "p2on2o4p42onmlkihfdba2cfiknop22nm8nop26",
    "p2nmn2o2p43o2nmljigdbabdfhknop20onl4m5nop26",
    "pom2n2o2p44onmlkjgca2bdfhkmop19nkj2k2l4m2nop26",
    "omlmn2op45o2nmljhca2bdfhkmop18nki3j2k2l3mnop26",
    "mk2mn2op46on2mkhdb3dfhkmop17nki5j2k2lm2nop26",
    "ihjlno2p46o2nmkhdb2cdehjmop16nkih2i4j2klmn2o2p25",
    "efilmo2p46o2nmkgdb2cdehjmo2p14nkih3i4j2klmno4p24",
    "cehkmnop46o2nmjfc4dehjmno8p6nki2h3i3j2kl2mno7p20o",
    "bdgjmnop46o2nliec4degjmno6nm3nopnki3h3i2j2kl2m2no11p15ol",
    "acfjlnop45o2n2liec3d2egjlno5nlifefkmki5h2i2jk2lm2n3o14p11ok",
    "acfilno2p44o2nmkhec3d2egjlno4nlhdb3ehi9jk2lm3n4o18p6nj",
    "abehkmnop43o3nmjgdc2d3egjlno3nlheba3beghi6jk2l2m5n12m7n3o4nj",
    "abdhjmno2p20op21o2n2ljgdc2d2efhjln2onmiecba4ceghi4j2k2l4m8l2k2j2i2h4gh2ij2kl3h",
    "abdgjlno3p17o5p17o3n2mkifcbcd2efhjln4kecb2a5bceghi2j2k3l9k2jihgfe2dc5d4e2fgfe",
    "abcfikmno28p3o12n2mlkhecbcd2efhjln3lgcb3a7bcegij2k3l4k3i2hgfd2c3b14c2b",
    "a2cfhklmn2o19n2o18n3mlkjgdb2cde2fhjln2lhdb4a9bcfhijk5jihged2c2b21a",
    "a2cegiklmn10o8n5o13n5m2lkjifdb2cdefghjlmlhdcb4a11bdfhj4ihfd2c10b8a3b5a",
    "babdfhjklm2nm3n14mn2o11n6m2lkjigecb2cdefghjkjgdcb6a12bdfghgf4e6d7c3b2a4b6",
    "babcegijkl2m8n9m3n4o6n5m4lkjigfdcb2cdefghj2fdcb2c2b7a10bc4de5d8c5b3a2bcd2cb2",
    "b2abcfhijk3l6m4n2m5lm2n12m4l2kjihfec2b2cdefhij2fedcb3a3bcdc4d5c6d11ed3cd6c4ef2ec2",
    "cba2bdfhi2j4k3l3m8l3m3n9m3l3kjihgedcb3cdefhij2hgfdcb2a3cef7g9h9g2h3gfgh2i5h2i3hdc",
    "cba3cefgh3i2j3k2l9k3l2m3n6m3l2k2jihged2c2bc2deghik2ji2gfe2dcdegh8i8j7ige2hj3kl4m8lkfd",
    "db2a2bcef3g2h2ij2k2l5k2j5k2l2m7l2k2j2ihgfed2c4d2fghjklk3j2i2hg3h5i5j11i2hedcdfghilmn12lge",
    "dcb4cd2e2f2g2hij2k4j2i2h5i2jk2l4ml2k2ji2hgfe2d4cd2efgikl3kj4i4h2i8j10ihfe2d4efgiln2o11mif",
    "e2dcbc2d2e3f2ghi2j4i2hg2f5g2h2ijk3l2k2ji2hg2fe10fhjklmlki2j5i2j10k5j2igfed5e2fghikno12njg",
    "gfed5e4f2gh2i3h2g2fed4c2d2efghi2j2kj2i2h2g2f2e5f4gijklmljhij2k3j2k2j6k6jhg2fe7f2ghijkmo2p9o2li",
    "g3fed2e3f3g2h5gf2edb3c2ba2bcdefg2hi5h3gf2ed2ef3g4hiklm2kj2k12j3k5ihg2f4ef4g2hi2jlmop12nk",
    "hg2f2e5fef2g5f2e2dca3bca5bcdefg2h4g3fd3e2fg7hikm4l3m4l3k4j2k4j2ih2g10h2ij2klmnp13o",
    "kigf2e6d2ef2g3fed2cba2b3a5b2cdefg4fe6fg3h8ikmn11mlkj2k4lk2j2i3h9i3jk2lmn2p14",
    "nkhf2e2d8ef3edc8ba4bc4def5ef2g4h7i4jln7o5nmlk5l2k2j9i5j2k2l2m2nop14",
    "pnkgfed7e6d2c2dfhgdc2bab3cd3e2f6g4h6i7jmno4n2o5n2ml3k2l3k5lm4k5l3m3n2op15",
    "p2mhfd2c3d2e7d2cegijhec3bcdce4f9g7h4i4jlno3n4o5n2m2l9k2lmn9o5p19",
    "p2okgd2c3d2e7d3hj3iged4e2f4g4f5g7h5i2jkmno3nl2mno4nm3l4m2l6mn2o6p25",
    "p3mhedc3d2e7d2eij4l2k2j2heg4h4g10h7i2jlno4mjkln2on2mlk4lm10no5p27",
    "p3okfdc3d2e7degij3kno2n2mjegh3i3h2g6h10i2kno5nl2mn4mkji2jl2m9n2o5p27",
    "p4mhec3d2e8fhj4kmn5kfgh2i2j2ih4g3h10ijlo3p2o2n6mljhghilm4n7o5p28",
    "p4njec3de8fgi5jk2l4jegh2ij3i2h3gfgh9i2kno5po5n2mkjh3jlmn10o4p29",
    "p3omiec2de3d5efgh6g7egh3jkj2i2hgfg2h8i2kmo6p2o4n2mkj4kmn10o4p30",
    "p2onkhec2defec5d3e2d4cde6h2g2jkj2i2gf2g2h9imno7po4nml3kl2mno2n2o9p31",
    "o3mkhec2defec7d4c3dgjkj2ihigf2ikji2hfd2efgh8ilno11n2m7no13p30o2",
    "o2nmkhec2defec7defe2dehkmlk2jihgd2gihgfedc2dfgh6g2hjmn4o6nm3nmn5o12p30o3",
    "n2m2khec2defec7deg2i3kl2ig4fecbc3bc6efgh4g4hkm2n3o4n12o11p21o2p6o5",
    "n2m2khec2defec7deghjlkl2kih3gedcba5b5cegh2g7hjklmn2o3n12o9p23o2no7n3",
    "m3lkhdc3de2c7dfghkml4k4ihfecba4b6dghg8hijkm2n2o2n2o4n6o3p30onmln6m2",
    "m3ljgdcb2cedc3ded2efhilm4n2m5lkigdba2b5cfihg9hijlmn4o14p31onm2l2m6",
    "l3khecbabcded3gi2h2jkmno7no3nmkjhfcb5cfi2g10hijlm4n3o3p3o5p32o2nm2l7",
    "k3jecba3bef3gil3mno2p8op3omkji2gecb2cfjihg10hik2lk3mn3o2p6o2p32o2n2m2l2k4",
    "kj2hdba4cfhijkmno2p16oljhg3edcbekmihg9h2ij2i3jln3o3p40on2m2l2k4",
    "i3gca5dhjlmnop19nlgdc3b4elnjh12i2jihijln4o2p40o2n2m2lkj3",
    "h2gfca5dilmo2p20nkgcba5bdknji8h2i8jlmn3o3p39o2n2m2lki3",
    "gf2eca5ejmnop20onkgcba5bcglji14ji3jkmn4o3p38o2n2m2lji3",
    "f2e2ba4bejmop21omjgcba6bek2ji12j3i2jkmnm2n2o2p38o3nm2kji3",
    "e3dba4bfjmop21omjfca8dk2j16i2jklm3n2o3p37o3nmlki4",
    "d3cba4bfkmop21omjfca8dk2j17i2klm4n2o3p36o2n2mkji4",
    "c3ba5cgkmo2p20olifca8cjk2j13k2jijklmnm3n2o3p34o3nmlkj5",
    "a9cgjlnop20nlifca8bfk3j3k12j3lmnl2mn3o2p34o2nmlkj6",
    "a9dgjlnop20nlieca9dl2k16j3km2l2m2n2o2p33o2nm2lkjk5",
    "a9cfikmop19onkheca9dklk14lk2j2km2l3m2n2op32o2n2mlkjk2l2ki",
    "a9cfhkmnop18onkheba9cjk11l6k4lmlkl2mno2p32on2mlkjkl4kh",
    "a9cehjlnop18omkheba9bejl18kl2m2lklmno2p31o2nmlk2jkm4kg",
    "a9begjlnop18omjgdba10dkl21m2k2lmno2p30o2nml2kjklm4ke",
    "a9bdgilnop17o2mjgda11dlml20m2k3mno2p29o2nm2lkj2lmn3mkd",
    "a9bdfikmop17onmjfca11dlml12m5l4mk2lmno2p29on2mlk2jkmn5kd",
    "a10cfhkmop16o2nlieca11dlml9m7lkj2klk2lmno2p28on2mlk2jklmn4mic",
    "a10cehkmop16on2lieba11dlml8m9kjijl2klmno2p27o2nmlk2j2kmn5lfb",
    "a10cehkmop8on2p5onmlieba11dlm2l7m9lkj2k3lmn2op17o5p4o2nml2kj2klmn5kda",
    "a10cegjmnop6nlk2np4onmkheba10bfmom2lm15lk5l2mnop15o8p2o2nml2kj3klmn4mkda",
    "a10bdgjmnop5nli2jmo5nmkhdba10dko2nm14n3m2l3kl2mnop15o11nml2kj4klm6jda",
    "b2a8bdgjlnop4omkji2jlm3nmlkhdba9bfmp2nm11n6m3l2k2lmnop15opo8nml2k2j5km6kfc",
    "c2ba7bcgjmop4onljhg2hij2km2kjhdba9dkop2nm10n7mlk5lmn2op16o9nmlkj3ij2kln5lfc",
    "d2cba7cgmop4on2mjecdefg2hklkigdba8bfmp3nm10n7mlkj3klm2nop16o6po2nmlkj2i3jklmn4lfd",
    "e2dca7cinp7oniedbcdf2gjkjigdba8dkop3nm10n7mkj4kl2mnop16o6po3mlkj2i3j2kmn4lge",
    "e2dca7dlp10nlidbcefgij2igdba7bfmp4onm8n9lkj3k2lmno2p17o8nmkji5jklm4kge",
    "e2dca7dmp11omhdbdfhijihfdba6belop5n2m7n9lk2j2k2lmno2p17o9nlji6jkl4kge",
    "e2dba7dlp12okgdeghijigedcb3a2bdkop6n18lk6lmno2p18o9mki6j2kl3jge",
    "ed2ba7dlop12mif2ijkjihfd3c4dfmp7n17ml2k5lmn2op19o8nlj7k2l3khe",
    "d3ba7dknop11olghjl2jihfd8gmp7on16ml2k5lm2nop20o2po5n3m4l6khe",
    "d3ba7cjmno3p8omj2lmlkigfdc7fmp8on12on2l3k5l2mnop28o4n3m2l4khe",
    "d2cba7cjm2n2o2p8omlmnmkigfdb3a3bdkop8on13mk8l2mno2p21op8o2n2m2l4khe",
    "d2cba7cilmn2o2p9nmn2mkigfcba6bfnp9on11mlk9lmno2p30o2n2m3l3kie",
    "c3a8chkm2no3p8ononmkjheca8dmp10on10lk10lmno2p21o2p8o2n2m2l4je",
    "c2ba8chklmn2o2p9o2nmljheca8dmp11on8mk5lk5lmn2op22op8o2n2m2l4je",
    "cb2a8bgjlmn2o2p10onmljheca8dmp12on6mk6lk5lmn2op22o2p7o2n2m2l4jf",
    "b2a9bfiklmno3p9o2mljheca8dmp13on3m2lk6l2k4lmn2o2p21o2p8o2n2m2l3kg",
    "a12dhjlmno3p10omljheca8dmp14on2mlk6l4k3lm2no2p21o3p7o2n2m2l3ki",
    "ba11cgikmn2o2p10nmkigeba8dmp15omlk7l2mlk3lm2n2op22o2p7o2n2m3l3k",
    "eba10bfiklmno2p10nmkigdba8dmp16nlk7l2m2lk2lm2n2o2p21o3p7o2nm3l4",
    "hca10behjlmno2p10omkhfca9dmp16nlk7lm4lklm3no2p21o2n2p6o2n2m2l4",
    "jda10behjlmno2p10omkheca9dmp16olk6l2m2n2l4m2n2o2p20o2n2op5o2n2m6",
    "jeba9bdgikmno2p10omkheba9dmp16olk6l2m2n2ml3m3no3p20on3p5o3n2m5",
    "kgba9bdgikln2op10omkhdba9dmp8o3p5omk5l2m2n3ml4m2n2o3p19o2n2op4o3n2m5",
    "lica9bdfikln2op10onkhdba8bfnp7ono2p6nlk4l2mn6m2l2m2n2o2p19o2n3o6n4m4",
    "lidb5a4bdfiklmno2p9onkheba8dkop6on3o2p5olk3l2m2n4o3nm5n2o2p19on2m2l4m2n4m3",
    "lhdc5ba4cfhjlmno2p9onlieca7bfmp5on7op5nlkl2m3n3o2p2o2nm4n2o2p18onm2l5m2n5m2",
    "kge6dba3behjlmno2p9onlifdb7dkop4onml5mnop4oml3m2n3o3p3o3nm3n2o2p17on2ml4m2n2o2n4",
    "jhg6fca3behjkmn2o2p8onljged7gmp5nji6jlnp5om4n3o4p4o3n2mn3o2p16on2m2l2mn2o4n4",
    "i8heba3dgiklmno2p8onljhf8hnp4njh7jmop6onm2n3o3p5o4n6op16o2nm4n2o6n3",
    "j8ifca3cgiklmno2p8onmkihg6hjop3olji6jknop7on4o4p5om2no2n3o3p15o2n2mn2o10p",
    "l7k2heba2bfhklmno2p9onljih5ijmop4nm8nop10o7p5nm2no8p15o2n4o2p10",
    "m5l4jfca2behjlmno3p8onmkj2i3jlnop5o4n4o3p11o5p5omlmno2p21o3no2p12",
    "onm6lkifcabdgilmno2p9onmlk4lmop10o8p12o3p6om3nop23o4p13",
    "ponm4l2kjhec2dfikmno2p8o2nlk4lnop13o5p21onm2nop24o2p15",
    "p2onml4k2igedegikmno2p7o2mlk3lmop17o2p22onmnop42",
    "p3onml2k2j2igf2hjlno2p7onmlk2lmo2p42on2o2p42",
    "p4om2l3kj2ihijlmno2p6onmkjklnop43o4p44",
    "p5o4nmlkjijklmno3p4onmkjiknp93",
))
IVORY = (0.84, 0.82, 0.77)
CREAM = (0.86, 0.79, 0.62)
GOLD = (0.78, 0.6, 0.24)
ROBE = {"": (0.1, 0.095, 0.1), "_red": (0.32, 0.04, 0.04), "_blue": (0.05, 0.1, 0.36)}
SHINE = {"w": 0.35, "c": 0.35, "p": 0.4, "g": 0.35, "b": 0.5, "t": 0.1, "d": 0.04, "o": 0.3}
CROSS = (351 / 768, 150 / 768)


def gold(k, nf):
    return [clamp(z * k * (0.85 + 0.3 * nf)) for z in GOLD]


def detail_fn(variant):
    n, fine, weave = Fbm(1311, 4, 5), Noise(1323, 256, 256), Noise(1325, 64, 512)

    def f(u, v):
        c = MAP.label(u, v)
        if c == "k":
            return [0, 0, 0, 0]
        x, y, e = u * 512, v * 512, MAP.edge(u, v)
        k = (0.88 + 0.24 * n.at(u * 6, v * 6)) * MAP.shade(u, v, 0.45, 3.0)
        nf = fine.at(x, y)
        if c in "wpgo":
            col = [clamp(z * k) for z in IVORY]
            if e < 1.3:
                col = gold(k, nf)                                # gold trim on every plate
            elif e < 1.7:
                col = [z * 0.55 for z in IVORY]                  # the trim's groove
            elif c == "p" and (y % 30) < 5:
                col = gold(k, nf)                                # the pauldron's banded lames
            elif c == "p" and (y % 30) < 7:
                col = [z * 0.5 for z in IVORY]
            elif c == "g" and (y % 48) < 6:
                col = gold(k, nf)                                # cuffs
            elif c == "o" and (y % 40) > 32:
                col = gold(k, nf)                                # toe caps
            elif abs(math.sin(x * 0.11) * 6 + math.sin(y * 0.07) * 6 - (x - y) % 24 + 12) < 0.9:
                col = [z * 0.78 for z in col]                    # engraved scrollwork
        elif c == "c":
            dx, dy = abs(u - CROSS[0]) * 768, abs(v - CROSS[1]) * 768
            if (dx < 5 and dy < 26) or (dy < 5 and dx < 17 and v * 768 < CROSS[1] * 768 + 4 and v * 768 > CROSS[1] * 768 - 10):
                col = gold(k * 1.1, nf)                          # the cross
            elif e < 1.3:
                col = gold(k, nf)
            else:
                col = [clamp(z * k) for z in CREAM]
        elif c == "b":
            col = gold(k, nf)
            if (x % 40) < 10 and 8 < (y % 24) < 16:
                col = [z * k for z in ROBE[variant]]              # set stones
            elif (y % 24) < 2:
                col = [z * 0.5 for z in GOLD]
        elif c == "t":
            col = [clamp(z * k * (0.9 + 0.12 * weave.at(u * 64, v * 16))) for z in CREAM]
            if e < 2.0:
                col = gold(k, nf)                                # the tabard's border
        else:   # robes
            fold = weave.at(u * 14, v * 3)
            col = [clamp(z * k * (0.65 + 0.7 * smooth(0.2, 0.8, fold)) * (0.92 + 0.16 * weave.at(u * 128, v * 512)))
                   for z in ROBE[variant]]
            if e < 1.2:
                col = gold(k * 0.8, nf)                          # gold-hemmed
        l, o = sample(LIT, u, v), 0.55 + 0.45 * sample(OPEN, u, v)
        shine = SHINE.get(c, 0.0) * l ** 8 * o
        return [clamp(z * (0.45 + 0.85 * l) * o + shine) for z in col] + [1]
    return f


# ---------------------------------------------------------------- the hood (uriel.tga, 768 px view)

TRIM = [((0.0, 0.73), (0.25, 0.62))]


def hood_detail_fn(variant):
    n, weave = Fbm(1327, 4, 5), Noise(1329, 64, 512)

    def f(u, v):
        if min(seg_dist(u, v, *s)[0] for s in TRIM) < 0.012:
            return gold(0.9, 0.5) + [1]                          # the hood's gold edge
        fold = weave.at(u * 16, v * 4)
        k = (0.45 + 1.0 * smooth(0.25, 0.75, fold)) * (0.85 + 0.3 * n.at(u * 4, v * 4))
        k *= 0.7 + 0.5 * smooth(0.9, 0.1, v)                     # lit from above
        return [clamp(z * k) for z in ROBE[variant]] + [1]
    return f


# ---------------------------------------------------------------- the face (uriel_h.tga, mirrored about u 0.27)

MID = 0.27
EYE = (0.4, 0.38, 0.045)


def face_detail_fn():
    n, strands = Fbm(1321, 3, 4), Noise(1331, 64, 256)

    def f(u, v):
        x = abs(u - MID)
        if x > 0.19 or v < 0.08:                                 # long black hair framing the face
            s = strands.at(u * 40, v * 140)
            k = 0.5 + 0.8 * smooth(0.3, 0.8, s)
            return [0.05 * k, 0.045 * k, 0.05 * k, 1]
        k = 0.88 + 0.16 * n.at(u * 3, v * 3)
        c = [0.86 * k, 0.8 * k, 0.78 * k]                         # pale skin
        c = [y * (1 - 0.25 * smooth(0.06, 0.0, abs(v - 0.5)) * smooth(0.06, 0.13, x)) for y in c]
        e = math.hypot(u - EYE[0], (v - EYE[1]) * 1.4)
        if e < EYE[2] * 1.6:
            c = [y * (0.3 + 0.7 * smooth(EYE[2] * 0.6, EYE[2] * 1.6, e)) for y in c]   # dark-lined eyes
        if abs(v - 0.6) < 0.012 and x < 0.08:
            c = [0.42, 0.14, 0.16]                               # lips
        if x < 0.012 and 0.16 < v < 0.27:
            c = [0.6, 0.06, 0.06]                                # the red mark on the brow
        top = smooth(0.02, 0.22, v)                              # the hood's shadow on the brow
        return [y * (0.4 + 0.6 * top) for y in c] + [1.0]
    return f


def look():
    return Look("lucy", "galena", {
        "body": Sheet({"": "angel", "_red": "angel", "_blue": "angel"},
                      detail={v: detail_fn(v) for v in ROBE}, size=1024),
        "hood": Sheet({"": "uriel", "_red": "uriel", "_blue": "uriel"}, model="uriel",
                      detail={v: hood_detail_fn(v) for v in ROBE}, size=256),
        "face": Sheet({"": "uriel_h", "_red": "uriel_h", "_blue": "uriel_h"}, model="uriel",
                      detail=face_detail_fn(), size=128),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_larm": "body", "u_rarm": "body"},
        "head": {"h_hood": "hood", "h_face": "face"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]},
        head_model="uriel")
