"""Scalebearer: keel's heavy armour under biker's head, the body painted
from scratch: a coat of big overlapping iron scales rimmed in copper over
the back, sides and pauldrons, a chest plate of banded lames with a
copper trim and a glowing gem, gauntlets in small scales, a leather
waist and a wide belt with hanging tassets, worn brown leather trousers
crossed by olive straps, red enamel knee pads, steel greaves and dark
boots with enamel toe caps. Shadows gather where one part meets another.
The head is a pale grey-blue brute with glowing blue eyes under a dark
helm rimmed in copper. Team versions enamel the pads and toe caps and
light the gem and the eyes in the team's colour."""
import math
from skins import Sheet, Look, LabelMap
from tremorart import Fbm, Noise, clamp, smooth, mix

# what each texel of keel.tga is, 128 x 128, run-length coded rows: s scale
# coat, p pauldrons, c chest plate, g gauntlets, w waist, t belt and tassets,
# u trousers, n knee pads, r greaves, o boots, k nothing
MAP = LabelMap((
    "o20s11o16k5w9s24u5n7u2t22u2t3uo",
    "o17s18o12k6w8s30n6ut27u2o",
    "o15s21os7k10w3s35n5t27u4",
    "k5s42k10s36n2u3t25u5",
    "k2s47k8s36ku12t12u10",
    "s49c3k5s36u35",
    "s48c6k3s36u35",
    "s47c7k3s36u35",
    "s46c8s39u35",
    "s32c21s40u35",
    "s30c22s40u36",
    "s29c22s41u36",
    "s27c23s42u36",
    "s25c25s42u36",
    "s23c27s42u36",
    "s19c31s42u36",
    "p5s14c31s41u37",
    "p5s14c29ps42u37",
    "p6s12c32s41u37",
    "up6s10c36s38u37",
    "up7s9c38s36u37",
    "up8s7c41s34u37",
    "up9s6c42s33u37",
    "up9s5c43s33u37",
    "u2p8s5c43s5ps27u37",
    "u2p9s3c44s5p4s24u37",
    "u2p9s3c44s5p4s24u37",
    "u2p10sc46s5p3s24u37",
    "u2p10sc46s5p3s24u37",
    "up12c46s5pg2s24u37",
    "up12c46s5g3s24u37",
    "up3g9c46s5g2s25u37",
    "g14c45s3g4s25u37",
    "g13c47s3g3s25u37",
    "g11s2c46s32u31n2u4",
    "g10s3c46s32u26n6u5",
    "g8s6c19s7c19s32u13n3u15nu5",
    "g7s10c11s14c16s33u15nu20g",
    "g6s40c6s40p3u11nu14t6g",
    "s91p10u16t11",
    "s91p10u14t13",
    "s90p12u15t11",
    "ts62ws25p14u16t9",
    "ts11w6s11w8s25w3s22p16u21t4",
    "ts6w34s14w13s17p21u19t3",
    "uw45s2w23s12p23u22",
    "uw78s3p23su22",
    "uw79p24s2u22",
    "u2w78p23s3u22",
    "u2w77p23s5u21",
    "u2w76p20g3s5u22",
    "u2w75p21g3s5u22",
    "u2w74p22g3s5u22",
    "u3w72p15gp6sg3s5u22",
    "u3w72p13g2p7sg3s5u22",
    "u3w72p10g4p8sg2s7u21",
    "u3w70p10g6p8sg2s7u21",
    "u2s3w64psp10g7p8sg3s7u21",
    "us6w3p5g10w36g2w3p4sp8g8p9sg3s7u21",
    "us10p4g20sw6g2w2g19p5sp6g9p10sg2s8u21",
    "s11p3g21s5g25p4s2p7g8p9s2g2s8u21",
    "s11p2g22s5g25p3s4p8g6p9s2g2s7t12u10",
    "s11pg23s5g25s14p4gp11s2gs7t13u10",
    "s11g24s4g26s20p9s3gs5t15u9s",
    "s11g23s5g26s23p6s9t17u6s2",
    "s11g23s5g25ps25g2ps10t18u4s3",
    "s11g23s4w2g24ps25g2ps9t21k2s3",
    "s11g23s4w2g23pgs26gs10t22ks3",
    "s11g23s4w2g26s36t22ks3",
    "s11g23w6g26s35t24s3",
    "s11g23w6g25s35t25s3",
    "s12g22w6g25s32t29s2",
    "s7o12g4o9g2w6g24s8w2s19kst31s2",
    "s5o15go12w3sw3g24s8w2s19k2t31s2",
    "so33w2s3wg23s8w4s17t34s2",
    "o35s4wg23s7w5s16t35so",
    "o36s3wg22s9w4s14t37o2",
    "o36s3wg22s7w7st2s6t41o2",
    "o36s3wg21s9w4s3t49o2",
    "o36s3wg21s10ws6t47u2o",
    "o36s3wg20s18t46u2o2",
    "o20u6o10s3wg20s18t44u4o2",
    "o19u11o7s2wg19s3c2s14t41u8o",
    "o18u12o13s3g17s15t40u9o",
    "o17u14o12s3w7g10s15t38u11o",
    "o16u14o13s3w6g11s15t36u13o",
    "o15u15o12s3gw6g11s15t34u15o",
    "o14u16o12s3gw6g11s15t33u16o",
    "o14u15no12s3gw6g11s15t31u19",
    "uo12u16n2o10s4g2w5g11s15t29u21",
    "uo12u13n5o10s3g3w5g10s16u23t4u23",
    "u2o10u12n7o10s3g3w5g10s12w4u24tu25",
    "u2o9u12n8o9s4g3w5g10s11w5u50",
    "u22n10o6nus4g2w5g11s11w5u50",
    "u22n17u2s2g3w5g10s12w5u50",
    "u30n9u2s2g3w5g10s17u50",
    "u29n10u2s2g3w5g10s17u50",
    "u29n10u2s2g3w5g10s17u50",
    "u29n10u2sg4w5g10s14w4u49",
    "u28n11u2sg4w5g10s14w4u49",
    "u28n2r5n4u2sg4w5g10s14w4u49",
    "u28r8n3u2wg5w4g10s14w4u49",
    "ur2u22r13u3swg5w3g11s13w5u48",
    "ur4u15r19u2s2wg5w3g10s14w4u48",
    "ur7u7r26s3g6w2g10s15w3u48",
    "ur41sw2g6w2g9s12w6u48",
    "ur41w4g6wg10s11w5u49",
    "r42ws3wg16s11w5u49",
    "r43s4wg15s11w5u49",
    "r43s5g15s11g6u48",
    "r44s5wg13s6gs5g5u48",
    "r43o2s3w2g13s6gs5w2g3u48",
    "r31o9r2o3s2w4g11s7g5w2g5u47",
    "r23o22sw6g10ws6g12u7nu39",
    "r8o38w6g10w2s5g12u4n9u34",
    "o8ro37w5g11w2s5g12u3n15u28o",
    "o47g16w2s4g12u5n13u28o",
    "o47g34u5n13u28o",
    "o47g8wg26u4n12u29o",
    "o47g8sg26u5n11u29o",
    "o47g8sg26u5n11u29o",
    "o47g8sg26u5n11u29o",
    "o47g8sg13s3w4g6u6n10u29o",
    "o47g20s3w7g4u7n10u28o2",
    "o47k2g6w5s6cs2w9g3u7n10u28o2",
    "o47k2g5w6s8w11g2u8n9u28o2",
    "o47k3g3w8s6w9s7u6n8u29o2",
    "o47k3g2w9s6w5s12u6n7u14t9u6ko",
))


IRON = (0.17, 0.18, 0.21)
STEEL = (0.32, 0.34, 0.38)
COPPER = (0.56, 0.3, 0.17)
LEATHER = (0.3, 0.19, 0.12)
DARK_LEATHER = (0.14, 0.09, 0.06)
OLIVE = (0.36, 0.36, 0.2)
GUNMETAL = IRON
ENAMEL = {"": (0.6, 0.08, 0.06), "_red": (0.62, 0.06, 0.05), "_blue": (0.08, 0.2, 0.62)}
LIGHT = {"": (0.3, 0.6, 1.0), "_red": (1.0, 0.3, 0.2), "_blue": (0.3, 0.6, 1.0)}
# how glossy each part is
SHINE = {"s": 0.35, "p": 0.4, "g": 0.25, "c": 0.3, "t": 0.15, "w": 0.05, "u": 0.06, "n": 0.4, "r": 0.4, "o": 0.1}
# the form shading over the body, worked out on the player's machine from
# the model they have (bake-form.py): (a + b x how much a texel faces the
# light) x (c + d x how open it is), and a highlight per material
FORM = {"body": {"model": ("keel", "default", "keel.tga"), "light": (0.45, 0.85), "open": (0.5, 0.5), "shine": SHINE, "label": MAP.label}}
GEM = (255 / 768, 159 / 768, 12 / 768)           # on the chest plate


def scales(u, v, nu, nv):
    """(depth 0..1 down a scale, distance to its rim 0..) for rows of
    overlapping scales, each row offset by half a scale."""
    y = v * nv
    row = math.floor(y)
    x = u * nu + 0.5 * (row % 2)
    fx, fy = x - math.floor(x) - 0.5, y - row
    shape = fy + 1.8 * fx * fx
    return clamp(shape), 1 - shape


def scale_paint(u, v, nu, nv, k, n):
    depth, rim = scales(u, v, nu, nv)
    if rim < 0.08:
        return [clamp(z * k * (0.8 + 0.4 * n)) for z in COPPER]
    if rim < 0.14:
        return [z * 0.3 for z in IRON]                       # the rim's shadow
    lit = 1.25 - 0.75 * depth                                # lit on top, dark at the foot
    if depth < 0.18:
        lit += 0.5 * (1 - depth / 0.18)                      # a glint along the scale's crown
    ridge = 1.15 if abs(((u * nu + 0.5 * (math.floor(v * nv) % 2)) % 1) - 0.5) < 0.04 else 1
    return [clamp(z * k * lit * ridge) for z in IRON]


def detail_fn(variant):
    n, fine, grain = Fbm(1411, 4, 6), Noise(1415, 256, 256), Noise(1421, 64, 512)

    def f(u, v):
        c = MAP.label(u, v)
        if c == "k":
            return [0, 0, 0, 0]
        x, y, e = u * 512, v * 512, MAP.edge(u, v)
        k = (0.85 + 0.3 * n.at(u * 6, v * 6)) * MAP.shade(u, v, 0.55, 3.5)
        nf = fine.at(x, y)
        if c == "s":
            col = scale_paint(u, v, 22, 30, k, nf)
        elif c == "p":
            col = scale_paint(u, v, 14, 18, k, nf)
        elif c == "g":
            col = scale_paint(u, v, 36, 48, k * 0.9, nf)
        elif c == "c":
            d = math.hypot(u - GEM[0], v - GEM[1]) / GEM[2]
            if d < 1:
                col = [z * (0.3 + 0.7 * (1 - d)) for z in LIGHT[variant]]
            elif d < 1.5:
                col = list(COPPER)                            # the gem's setting
            elif e < 1.6:
                col = [clamp(z * k * (0.8 + 0.4 * nf)) for z in COPPER]   # trim
            else:
                lame = (y % 26) / 26.0
                col = [clamp(z * k * (1.2 - 0.6 * lame) * (0.92 + 0.16 * grain.at(u * 16, v * 128)))
                       for z in IRON]
                if lame < 0.07:
                    col = [z * 0.25 for z in IRON]           # the gap between lames
                elif (x % 26) < 3 and 0.12 < lame < 0.24:
                    col = list(COPPER)                        # rivets
        elif c == "w":
            col = [clamp(z * k * (0.85 + 0.3 * nf)) for z in LEATHER]
            if (y % 18) < 2:
                col = [z * 0.5 for z in col]                  # stitched bands
        elif c == "t":
            band = (y % 40) / 40.0
            if band < 0.4:
                col = [clamp(z * k * (0.85 + 0.3 * nf)) for z in DARK_LEATHER]   # the belt
                if band < 0.06 or band > 0.34:
                    col = list(COPPER)
            else:
                col = scale_paint(u, v, 18, 12.8, k, nf)       # tassets hanging below
        elif c == "u":
            crease = abs(grain.at(u * 10, v * 70) - 0.5) < 0.02
            col = [clamp(z * k * (0.8 + 0.4 * nf) * (0.6 if crease else 1)) for z in LEATHER]
            if nf > 0.75:
                col = [clamp(z * 1.3) for z in col]           # scuffed
            strap = min(abs((x + y) % 150 - 75), abs((x - y) % 170 - 85))
            if strap < 12:
                col = [clamp(z * k * (0.85 + 0.3 * nf)) for z in OLIVE]
                if strap > 10.5:
                    col = [z * 0.5 for z in col]
                elif (x % 30) < 3:
                    col = list(STEEL)                         # rivets on the straps
        elif c == "n":
            if e < 0.8:
                col = list(STEEL)                             # the pad's steel rim
            elif nf > 0.86:
                col = [0.7, 0.68, 0.66]                       # chipped enamel
            else:
                col = [clamp(z * k * (1.1 - 0.3 * v % 1)) for z in ENAMEL[variant]]
        elif c == "r":
            col = [clamp(z * k * (0.9 + 0.2 * grain.at(u * 16, v * 128))) for z in STEEL]
            if (y % 32) < 2:
                col = [z * 0.4 for z in col]                  # plate seams
            elif (y % 32) < 5 and (x % 22) < 3:
                col = list(COPPER)
        else:   # boots
            if (y % 64) > 44:
                col = [clamp(z * k) for z in ENAMEL[variant]]  # the toe cap
                if nf > 0.8:
                    col = [0.7, 0.68, 0.66]
            else:
                col = [clamp(z * k * (0.8 + 0.4 * nf)) for z in DARK_LEATHER]
                if (y % 10) < 2 and abs(x % 40 - 20) < 6:
                    col = [0.5, 0.45, 0.4]                    # laces
        return [clamp(z) for z in col] + [1]
    return f


def glow_fn(colour):
    def f(u, v):
        d = math.hypot(u - GEM[0], v - GEM[1]) / GEM[2]
        k = clamp(1.2 * (1 - d))
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


def neck_detail(u, v):
    return list(IRON) + [0.9]


# ---------------------------------------------------------------- the head (biker_h.tga, 768 px view)

EYES = [(115, 402), (252, 398)]
SKIN = (0.6, 0.66, 0.74)
def helm(x, y):
    """How far (x, y) is inside the helm, in px: the brow and the back of the head."""
    return max(320 - y, x - 390)


def head_grade(u, v):
    x, y = u * 768, v * 768
    if helm(x, y) > 0:
        return [0.4, 0.4, 0.45, 1]
    return [0.62, 0.68, 0.8, 1]                       # warm skin to a cold grey-blue


def head_detail_fn():
    n = Fbm(1419, 4, 6)

    def f(u, v):
        x, y = u * 768, v * 768
        h = helm(x, y)
        k = 0.8 + 0.4 * n.at(u * 8, v * 8)
        if h > 14:
            k *= (0.7 + 0.3 * smooth(14, 60, h)) * (1.5 - 0.9 * smooth(0, 700, y + max(0, x - 390)))
            if abs(x - 150) < 40 and y < 300:
                k *= 1 + 0.8 * (1 - abs(x - 150) / 40)       # light along the dome
            ridge = abs(x - 185) < 10 and y < 300         # a ridge down the brow
            return [clamp(z * k * (2.2 if ridge else 1)) for z in GUNMETAL] + [1]
        if h > 0:
            return list(COPPER) + [1]                      # the helm's rim
        for ex, ey in EYES:
            d = math.hypot((x - ex) / 34, (y - ey) / 16)
            if d < 1:
                return [0.02, 0.03, 0.06, 1 - 0.5 * d]     # a dark socket round the light
        return [clamp(z * k) for z in SKIN] + [0.45]
    return f


def head_glow_fn(colour):
    def f(u, v):
        x, y = u * 768, v * 768
        k = 0
        for ex, ey in EYES:
            k = max(k, 1 - math.hypot((x - ex) / 22, (y - ey) / 10))
        k = clamp(k * 1.6)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


def look():
    return Look("keel", "scalebearer", {
        "body": Sheet({"": "keel", "_red": "keel", "_blue": "keel"},
                      detail={v: detail_fn(v) for v in ENAMEL},
                      glow={v: glow_fn(LIGHT[v]) for v in LIGHT}, size=1024, form=True),
        "neck": Sheet({"": "keel_h", "_red": "keel_h", "_blue": "keel_h"}, detail=neck_detail, size=64),
        "head": Sheet({"": "biker_h", "_red": "biker_h", "_blue": "biker_h"}, model="biker",
                      grade=head_grade, detail=head_detail_fn(),
                      glow={v: head_glow_fn(LIGHT[v]) for v in LIGHT}, size=256),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_neck": "neck"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]},
        head_model="biker")
