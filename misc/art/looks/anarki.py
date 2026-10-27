"""Anarki in his own colours: a tanned, scarred junk-rat instead of the
grey skin, a near-black mohawk, the green shorts and olive trousers turned
to worn brown leather, rusted scrap armour, and brass-rimmed goggles. Team
versions keep the team's paint with the tan, the dark hair and the brass
goggles."""
from skins import Sheet, Look, region, inside
from tremorart import Fbm

# anarki.tga, 768 px view
SKIN = [region(p, 768, 768) for p in (
    [(0, 0), (345, 0), (345, 235), (135, 235), (135, 520), (0, 520)],
    [(0, 420), (215, 420), (215, 768), (0, 768)],
)]
SHORTS = [region([(135, 235), (355, 235), (355, 520), (135, 520)], 768, 768)]
TROUSERS = [region([(490, 15), (765, 15), (765, 390), (490, 390)], 768, 768)]

# anarki_h.tga, 768 px view
HAIR = [region(p, 768, 768) for p in (
    [(0, 0), (768, 0), (768, 230), (0, 230)],
    [(420, 0), (768, 0), (768, 300), (650, 455), (440, 470), (415, 380)],
)]


def body_grade(u, v):
    if inside(SKIN, u, v):
        return [0.62, 0.48, 0.37, 1]
    return [0.58, 0.46, 0.4, 1]          # scrap with a rust cast


def body_detail_fn(team):
    n = Fbm(2113, 4, 6)

    def f(u, v):
        if not team and (inside(SHORTS, u, v) or inside(TROUSERS, u, v)):
            k = 0.8 + 0.4 * n.at(u * 8, v * 8)
            return [0.28 * k, 0.17 * k, 0.1 * k, 0.7]
        return [0, 0, 0, 0]
    return f


def head_grade(u, v):
    if inside(HAIR, u, v):
        return [0.3, 0.27, 0.27, 1]
    return [0.62, 0.48, 0.37, 1]


def goggles_grade(u, v):
    return [0.72, 0.55, 0.3, 1]


def look():
    return Look("anarki", "anarki", {
        "body": Sheet({"": "anarki", "_red": "red", "_blue": "blue"},
                      grade=body_grade,
                      detail={"": body_detail_fn(False), "_red": None, "_blue": None}, size=256),
        "head": Sheet({"": "anarki_h", "_red": "red_h", "_blue": "anarki_h"}, grade=head_grade, size=128),
        "goggles": Sheet({"": "anarki_g", "_red": "red_g", "_blue": "blue_g"},
                         grade={"": goggles_grade, "_red": None, "_blue": None}, size=64),
    }, {"lower": {"l_legs": "body", "l_board": "models/players/anarki/anarki_b.tga"},
        "upper": {"u_torso": "body"},
        "head": {"h_head": "head", "h_glasses": "goggles"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
