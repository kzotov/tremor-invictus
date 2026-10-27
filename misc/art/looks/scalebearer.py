"""Scalebearer: keel's heavy armour under biker's head. The armour is
dark gunmetal over the model's own shading, laid with overlapping scales
rimmed in copper; the undersuit is brown leather, the old gore red
enamel plates, scratched; the head is a pale grey-blue brute with
glowing blue eyes under a dark helm rimmed in copper. Team versions
enamel the plates and light the eyes in the team's colour."""
import math
from skins import Sheet, Look, LabelMap
from tremorart import Fbm, Noise, clamp, smooth

# what each texel of keel.tga is, 128 x 128, run-length coded rows:
# a armour, u undersuit, g gore, k shadow
MAP = LabelMap((
    "k61u5a13u19a25u3g2",
    "k59u6a16u16a26u3g2",
    "k11u9k12u6k20u6a25u8a26u3g2",
    "k8u13k10u12k15u5a27u6a28u3g",
    "k4u10a2u6k8u16k12u4a29u5a28u3g",
    "k2u8a8u5k6u5a7u8k9u3a31u3a29u4",
    "u7a12u5k5u4a11u8k5u4a64u3",
    "u4a16u5k3u4a15u13a65u3",
    "a21u10a19u10a65u3",
    "a21u10a22u7a65u3",
    "a22u8a26u3a66u3",
    "a23u6a97u2",
    "a25u2a99u2",
    "a126u2",
    "a127u",
    "a128",
    "a128",
    "a128",
    "a128",
    "a128",
    "a128",
    "a128",
    "a128",
    "a92u4a32",
    "a91u6a31",
    "a90u7a31",
    "a89u8a14g2a15",
    "a89u8a13g4a14",
    "a90u7a12ug4a14",
    "a91u6a12ug4a14",
    "a91u5a13u2g3a14",
    "a91u4a14u2g3a14",
    "a91u4a14u2g3a14",
    "a91u5a13u2g3a12u2",
    "a91u6a12ug4a11u3",
    "a66u6a18u8a11g5a9u4g",
    "a65u9a16u10a8ug5ua7u5g",
    "a64u11a16u12a4u9a5u7",
    "a63u14a19u21a4u7",
    "a60u20a17u21a2u8",
    "a58u26a14u30",
    "a57u28a13u5k4u21",
    "a56u29a14u3k6u20",
    "a56u28a15u3k6u20",
    "a55u28a17u3k5u20",
    "a54u28a18u3k5u20",
    "a53u28a20u3k4u20",
    "a52u29a20u3k4u20",
    "ua18u2a10u2a17u30a21u4k2u21",
    "u3a15u4a8u4a4u3a7u32a22u26",
    "u4a6u13a6u13a5u32a23u26",
    "u23a6u50a23u26",
    "u23a7u49a24u13k5u7",
    "u23a7u49a24u12k7u6",
    "u22a8u49a24u12k9u4",
    "u22a8u49a24u11k11u3",
    "ua3u18a7u50a24u10k15",
    "a5u16a7u19a4u28ga23u5k20",
    "a6u10a12u17a14u21ga22u4k21",
    "ua28u15a17u21a21u4k21",
    "u2a28u13a19u4k2u15a20u4k21",
    "u2a30u10a20u22a19u4k21",
    "u2a32u7a22u23a17u4k21",
    "ua35u4a23u25a15u4k21",
    "a63u12a2u14a11u4k22",
    "a64u10a4u17a6u5k8u4k10",
    "a65u9a4u27k8u7k8",
    "a66u7a5u27k6u10k7",
    "ua67u3a6u28k5u5a3u3k7",
    "u2a44g14a15u18k3u9k3u5a6u3k6",
    "u3a11u9a11u4a3g20a13u18k7u13a7u3k6",
    "u8a4ug2u9g5a2u7g25a11u18k8u10a10u3k5",
    "u7g9u7g9u5g27a9u19k8u7a13u3k5",
    "k2u4g11u6g10u4g28a8u19k7u6a15u4k4",
    "k3u3g13u4g11u4g29a5u21k2u8a18u3k4",
    "k3u2g15u2g12u5g30a2u31a20u3k3",
    "k2u2g30u6g30u30a22u3k3",
    "ku2g31u12g24u26a27u3k2",
    "u2g32u5k6u3g12u3g6u4ku6a43u3k2",
    "u2g32u4k10ug8u13k5u4a44u3k2",
    "ug34u3k12u2g2u3k8u3k7u3a45u3k2",
    "ug20a3g12u3k36u3a44u4k2",
    "ug19a6g12u3k34u3a43u5k2",
    "ug18a9g11u3k33u3a43u5k2",
    "ug17a11g11u3k18u8k6u3a44u5k",
    "ug16a5u2a5g12u2k6u23k2u4a45u4k",
    "ug15a4u6a4g12u2k3u16a4u11a47u3",
    "u2g13a4u8a3g12u19a7u10a48u2",
    "u3g11a4u10a2g12u13a14ug3u5a49u",
    "u4g9a4u11a2ug12u7a20g4u4a50",
    "u10g2ua4u10a4g13u5a21g5u4a49",
    "u13a5u7a6ug12u4a22g5u6a5u4a38",
    "u13a19g11u5a21g6u17a36",
    "a2u10a22u2a2u10a20g7u18a35",
    "a5u6a28u9a18ug8u2k3u14a34",
    "a40u9a16g10uk5u8k3u3a33",
    "a40u10a15g10uk5u7k5u3a32",
    "a40u13a13g9uk4u8k6u4a30",
    "a40u15a11g9uk4u8k7u8a25",
    "a39u18a9g9uk3u10k10u5a24",
    "ua38u19a9g7u2k3u10k13u3a23",
    "ua38u17g2ua10g5u2k3u4a2u4k14u2a23",
    "u2a37u16g3u2a10ug3u2k3u3a4u3k15u2a22",
    "u2a37u16g4u2a9u2gu3k3u3a4u3k15u2a21u",
    "u2a37u16g5u2a7u6k4u3a4u3k16u2a19u2",
    "u2a37u17g4u3a5u7k4u3a4u3k16u2a19u2",
    "u3a17u5a14u36k4u3a5u2k16u3a17u3",
    "u3a16u6a14u35k5u3a5u3k6u5k3u4a17u2k",
    "u4ag3a11u2a18u34k7u2a5u3k4u14a16u3k",
    "u4g7a6g6a16u5k2u27k7u2a5u4k2u4a3u8a16u2k2",
    "u3g9a3g9a9ga4u5k5u25k7u2a5u9a5u7a16u2k2",
    "u3g22a7g4au6k7u11g6u7k6u3a4u8a7u5a16u3k2",
    "u3g23a6g7u4k8u8g10u6k5u3a4u8a7u5a16u2k3",
    "u3g24a5g8u2k9u6g15u3k5u3a4u9a5u6a16u2k3",
    "u4g24a4g8u2k9u5g17u3k4u4a2u11a3u7a16u2k3",
    "u5g23a4g7u3k9u4g19u2k5u25a16u2k4",
    "u6g23a2g7u3k10u3g20u2k5u25a16u2k4",
    "u7g30u3k11u3g20u2k5u25a16u2k4",
    "u7g30u2k12u3g20u2k5u26a15u2k4",
    "uk2u4g30u2k12u3g20u2k5u26a15u2k4",
    "k4u4g29u2k13u3g19u2k6u25a15u2k4",
    "k5u5g28uk13u4g17u3k6u25a14u3k4",
    "k5u6g27uk14u3g16u3k7u25a14u2k5",
    "k4u7g27uk14u4g14u4k7u25a14u2k5",
    "k3u2g4ug27u2k15u4g7u9k8u25a13u3k5",
    "u4g11u2g19u2k16u5g3u12k8u26a12u3k5",
    "gu2g11u4g14u5k17u19k9u26a11u3k6",
    "g14u5g12u4k20u18k9u26a11u3k6",
))
STEEL = (0.4, 0.43, 0.52)                        # the grade over the armour
GUNMETAL = (0.15, 0.16, 0.19)
COPPER = (0.62, 0.32, 0.18)
LEATHER = (0.3, 0.2, 0.13)
ENAMEL = {"": (0.55, 0.08, 0.06), "_red": (0.6, 0.06, 0.05), "_blue": (0.08, 0.2, 0.6)}


def scales(u, v, nu, nv):
    """(depth 0..1 down a scale, on its rim) for overlapping rows of scales."""
    y = v * nv
    row = math.floor(y)
    x = u * nu + 0.5 * (row % 2)
    fx, fy = x - math.floor(x) - 0.5, y - row
    shape = fy + 1.6 * fx * fx
    return clamp(shape), shape > 0.9


def body_grade_fn():
    n = Fbm(1413, 4, 6)

    def f(u, v):
        c, k = MAP.label(u, v), MAP.shade(u, v, 0.5)
        k *= 0.88 + 0.24 * n.at(u * 5, v * 5)
        if c == "a":
            return [clamp(z * k) for z in STEEL] + [1]
        if c in "ug":
            return [0.42 * k, 0.36 * k, 0.32 * k, 1]
        return [0.5 * k, 0.5 * k, 0.5 * k, 1]
    return f


def body_detail_fn(variant):
    n, fine = Fbm(1411, 4, 6), Noise(1415, 256, 256)

    def f(u, v):
        c, e = MAP.label(u, v), MAP.edge(u, v)
        k = (0.8 + 0.4 * n.at(u * 8, v * 8)) * MAP.shade(u, v, 0.4)
        if c == "a":
            if e < 1.2 and fine.at(u * 400, v * 400) > 0.5:
                return list(COPPER) + [0.75]                  # copper rim worn bright
            depth, rim = scales(u, v, 20, 26)
            if rim:
                return list(COPPER) + [0.5]
            k *= 1.15 - 0.6 * depth                           # each scale shadowed at its foot
            return [clamp(z * k) for z in GUNMETAL] + [0.75]
        if c == "g":
            if fine.at(u * 120 + 5, v * 120) > 0.8:
                return [0.75, 0.72, 0.68, 0.6]                # chipped enamel
            return [clamp(z * k) for z in ENAMEL[variant]] + [0.9]
        if c == "u":
            if abs(fine.at(u * 20, v * 140) - 0.5) < 0.015:
                return [0.12, 0.08, 0.05, 0.6]                # a crease in the leather
            return [clamp(z * k) for z in LEATHER] + [0.7]
        return [0, 0, 0, 0]
    return f


def neck_detail(u, v):
    return list(GUNMETAL) + [0.85]


# ---------------------------------------------------------------- the head (biker_h.tga, 768 px view)

EYES = [(115, 402), (252, 398)]
SKIN = (0.6, 0.66, 0.74)
LIGHT = {"": (0.3, 0.6, 1.0), "_red": (1.0, 0.3, 0.2), "_blue": (0.3, 0.6, 1.0)}


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
            k *= 0.7 + 0.3 * smooth(14, 60, h)
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
        "body": Sheet({"": "keel", "_red": "keel", "_blue": "keel"}, grade=body_grade_fn(),
                      detail={v: body_detail_fn(v) for v in ENAMEL}, size=512),
        "neck": Sheet({"": "keel_h", "_red": "keel_h", "_blue": "keel_h"}, detail=neck_detail, size=64),
        "head": Sheet({"": "biker_h", "_red": "biker_h", "_blue": "biker_h"}, model="biker",
                      grade=head_grade, detail=head_detail_fn(),
                      glow={v: head_glow_fn(LIGHT[v]) for v in LIGHT}, size=256),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_neck": "neck"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]},
        head_model="biker")
