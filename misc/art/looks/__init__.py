"""The champions' looks on models the player has (see skins.Look), one
module each, each with a look()."""
import importlib

MODULES = ("nyx", "deathknight", "athena")


def ALL():
    """(module, look) for every champion's look."""
    out = []
    for name in MODULES:
        module = importlib.import_module("looks." + name)
        out.append((module, module.look()))
    return out
