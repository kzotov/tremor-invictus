#!/usr/bin/env python3
"""Sync the external invictus-hud-gradients-v1 pack over generated HUD art.

Usage: import-hud-art.py sync|status GENERATED [--pack PACK]
Default: $TREMOR_HUD_ART, or $TREMOR_LOCAL/branding/invictus-hud-gradients-v1.
No dependencies beyond Python 3. Verifies checksums, paths, TGA dimensions and hash-bound visual anchors.
Absent pack restores the generated art; malformed pack restores it and fails.
The branding and ability packs are independent and are never touched here.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import sys

PACK_NAME = 'invictus-hud-gradients-v1'
STATE = 'hud-art'
GROUPS = {'weapons': 12, 'ammo': 7, 'passives': 32, 'items': 22,
          'causes': 8, 'glyphs': 10, 'medals': 12}
PREFIX = 'gfx/tremor/icons/'
ANCHORS = PREFIX + 'anchors.txt'


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def default_pack():
    local = Path(os.environ.get('TREMOR_LOCAL', str(Path.home() / 'projects/quake/local')))
    return Path(os.environ.get('TREMOR_HUD_ART', str(local / 'branding' / PACK_NAME)))


def safe(name):
    return bool(re.fullmatch(r'gfx/tremor/icons/(weapons|ammo|passives|items|causes|glyphs|medals)/[a-z0-9_]+\.tga', name))


def check_pack(pack):
    if not (pack / 'manifest.json').is_file():
        raise ValueError('missing manifest.json')
    sums = json.loads((pack / 'runtime-checksums.json').read_text())
    if not isinstance(sums, dict):
        raise ValueError('checksums must be an object')
    counts = dict.fromkeys(GROUPS, 0)
    for name, expected in sums.items():
        if not safe(name) or not isinstance(expected, str) or not re.fullmatch('[0-9a-f]{64}', expected):
            raise ValueError('invalid path or checksum: ' + str(name))
        p = pack / 'runtime' / name
        if not p.is_file() or digest(p) != expected:
            raise ValueError('missing or modified texture: ' + name)
        head = p.read_bytes()[:18]
        size = (256, 128) if name.startswith(PREFIX + 'weapons/') else (128, 128)
        if len(head) != 18 or head[2] not in (2, 10) or head[16] != 32 or struct.unpack('<HH', head[12:16]) != size:
            raise ValueError('invalid TGA dimensions/format: ' + name)
        counts[name.split('/')[-2]] += 1
    if counts != GROUPS:
        raise ValueError('incomplete pack: expected %s, found %s' % (GROUPS, counts))
    anchor_text(pack, sums)  # requires complete, matching placement metadata
    return sums


def anchor_text(pack, sums):
    icons = json.loads((pack / 'alignment/anchors.json').read_text())['icons']
    if not isinstance(icons, dict) or set(icons) != set(sums):
        raise ValueError('anchors must cover every runtime texture exactly once')
    lines = ['// Hash-validated optical anchors: shader_path u v; apply once.']
    for name, expected in sorted(sums.items()):
        entry = icons[name]
        uv = entry['anchor_uv']
        size = [256, 128] if name.startswith(PREFIX + 'weapons/') else [128, 128]
        if entry['sha256'] != expected or entry['texture_size'] != size:
            raise ValueError('stale anchor: ' + name)
        if not isinstance(uv, list) or len(uv) != 2 or not all(
                isinstance(v, (int, float)) and math.isfinite(v) and 0 < v < 1 for v in uv):
            raise ValueError('invalid anchor coordinates: ' + name)
        lines.append('%s %.6f %.6f' % (name[:-4], uv[0], uv[1]))
    return ('\n'.join(lines) + '\n').encode()


def read_state(generated):
    p = generated / STATE / 'installed.json'
    if not p.is_file():
        return {}
    data = json.loads(p.read_text())
    if not isinstance(data, dict) or not all(safe(n) or n == ANCHORS for n in data):
        raise ValueError('invalid installed HUD state')
    return data


def write_changed(path, data):
    if path.is_file() and path.read_bytes() == data:
        return 0
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return 1


def revert(generated, entries):
    for name, expected in entries.items():
        p = generated / name
        if name == ANCHORS:
            if p.exists(): p.unlink()  # never apply painted anchors to restored placeholders
            continue
        if p.is_file() and digest(p) != expected:
            continue  # regenerated or externally changed since sync
        backup = generated / STATE / 'drawn' / name
        if backup.is_file():
            p.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(backup, p)
        elif p.is_file():
            p.unlink()
    state = generated / STATE / 'installed.json'
    if state.exists():
        state.unlink()


def sync(generated, pack):
    entries = read_state(generated)
    if not pack.is_dir():
        revert(generated, entries)
        print('import-hud-art: no pack at %s; using generated HUD art' % pack, file=sys.stderr)
        return 0
    try:
        sums = check_pack(pack)
    except (OSError, ValueError, TypeError, KeyError) as e:
        revert(generated, entries)
        print('import-hud-art: %s; restored generated HUD art' % e, file=sys.stderr)
        return 1
    anchors = anchor_text(pack, sums)
    files = dict(sums)
    files[ANCHORS] = hashlib.sha256(anchors).hexdigest()
    # Revert removed entries before installing a revision of the pack.
    removed = {n: d for n, d in entries.items() if n not in files}
    if removed: revert(generated, removed)
    changed = 0
    for name, expected in sorted(sums.items()):
        p = generated / name
        if p.is_file() and digest(p) != expected and (name not in entries or digest(p) != entries[name]):
            backup = generated / STATE / 'drawn' / name
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, backup)
        changed += write_changed(p, (pack / 'runtime' / name).read_bytes())
    changed += write_changed(generated / ANCHORS, anchors)
    write_changed(generated / STATE / 'installed.json', (json.dumps(files, indent=2) + '\n').encode())
    if changed:
        print('import-hud-art: %s (%d files updated)' % (PACK_NAME, changed))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('sync', 'status'))
    parser.add_argument('generated', type=Path)
    parser.add_argument('--pack', type=Path, default=default_pack())
    args = parser.parse_args()
    if args.command == 'sync':
        return sync(args.generated.resolve(), args.pack.expanduser().resolve())
    entries = read_state(args.generated)
    if len(entries) == sum(GROUPS.values()) + 1 and ANCHORS in entries and all((args.generated / n).is_file() and digest(args.generated / n) == d for n, d in entries.items()):
        print('painted ' + PACK_NAME)
        return 0
    print('generated')
    return 3


if __name__ == '__main__':
    sys.exit(main())
