"""The champions' looks on models the player has (see skins.Look), one
module each, each with a look():

  models/players/mynx/*_nyx[_red|_blue].skin, nyx/head_*.skin, nyx_*.tga
                       Nyx on mynx (looks/nyx.py): cyan-blue hair, a
                       navy-teal suit with cyan seams, dark glasses with a
                       cyan glint, a crescent on the shoulder
  models/players/visor/*_deathknight*.skin, bones/head_deathknight.skin, ...
                       Death Knight on visor's body with bones' skull
                       (looks/deathknight.py): blackened plate with embers in
                       its cracks, a furnace grate on the shoulder, a charred
                       skull with fire in the sockets
  models/players/crash/*_athena*.skin, major/head_athena.skin, ...
                       Athena on crash's body with major's head
                       (looks/athena.py): an olive military uniform with
                       sage lights, dark hair
  models/players/bitterman/*_bj*.skin, ...
                       B.J. on bitterman (looks/bj.py): a human skin over the
                       hide, a blond buzz cut over the tentacles, olive vest
                       and fatigues, a leather belt
  models/players/sarge/*_eisen*.skin, ...
                       Eisen on sarge (looks/eisen.py), painted from scratch:
                       a red quilted vest over a yellow shirt, leather chaps
                       over khaki trousers, grey boots, ginger hair and beard
  models/players/lucy/*_galena*.skin, uriel/head_galena.skin, ...
                       Galena on lucy's "angel" plate with uriel's hood
                       (looks/galena.py): cold iron with teal seams, dark
                       robes, a charcoal hood with a teal trim, a pale face
                       with teal eyes
  models/players/keel/*_scalebearer*.skin, biker/head_scalebearer.skin, ...
                       Scalebearer on keel's armour with biker's head
                       (looks/scalebearer.py): copper-rimmed gunmetal scales,
                       brown leather, red enamel plates, a grey-blue face with
                       glowing blue eyes under a dark helm
  models/players/major/*_raine*.skin, ...
                       Raine on major (looks/raine.py): a slate survivor's
                       suit with blue lights, the hair darkened
  models/players/uriel/*_beastmaster*.skin, ...
                       Beastmaster on uriel (looks/beastmaster.py): no wings,
                       a bronze hide with blue war paint, a dark pelt hood,
                       amber eyes
  models/players/orbb/*_unumrey*.skin, ...
                       Unumrey on orbb (looks/unumrey.py): bruised violet
                       flesh, blackened metal, an eye of our own with a
                       glowing violet ring
  models/players/sorlag/*_sorlag*.skin, ...
                       Sorlag (looks/sorlag.py): an olive-green hide with
                       glowing acid glands and a salmon frill
  models/players/doom/*_doomslayer*.skin, ...
                       Doom Slayer (looks/doom.py): deep green armour and
                       helmet, a dark grey undersuit, a gold visor
  models/players/tankjr/*_clutch*.skin, ...
                       Clutch on tankjr (looks/clutch.py): dark red armour,
                       steel plates over the emblems, steel feet, a
                       steel-cased head with amber eyes
  models/players/ranger/*_ranger*.skin, ...
                       Ranger (looks/ranger.py): olive-drab vest, pads and
                       helmet over a dark undersuit, a brass buckle
  models/players/anarki/*_anarki*.skin, ...
                       Anarki (looks/anarki.py): tanned skin, a near-black
                       mohawk, brown leather, rusted scrap, brass goggles
  models/players/keel/*_keel*.skin, ...
                       Keel (looks/keel.py): tan armour and forearms, a
                       brown undersuit at the neck, bloodied boots
  models/players/slash/*_slash*.skin, ...
                       Slash (looks/slash.py): olive-gold corset, orange
                       shorts, olive legs, dark pads, dark hair, green
                       skates
  models/players/visor/*_visor*.skin, ...
                       Visor (looks/visor.py): orange armour over pale
                       skin, an orange face mask with a dark slit
"""
import importlib

MODULES = ("nyx", "deathknight", "athena", "bj", "eisen", "galena", "scalebearer", "raine", "beastmaster",
           "unumrey", "sorlag", "doom", "clutch", "ranger", "anarki", "keel", "slash", "visor")


def ALL():
    """(module, look) for every champion's look."""
    out = []
    for name in MODULES:
        module = importlib.import_module("looks." + name)
        out.append((module, module.look()))
    return out
