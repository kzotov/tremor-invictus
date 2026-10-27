"""Render a sheet of player-model combinations: every body with every head,
straight from the game's packs, to choose a "body:head" model for a
champion (g_championModels, bg_championList).

  python3 misc/tools/model-combos.py BASEQ3_DIR OUT.png \\
      --bodies grunt biker/default tankjr --heads grunt visor tankjr

BASEQ3_DIR holds the .pk3 files (later names win, as in the game). Each
cell shows the body standing in its idle pose with the other head on its
tag, from the front three-quarters, lit simply. Needs numpy and Pillow.
"""
import argparse, io, math, os, struct, zipfile
import numpy as np
from PIL import Image, ImageDraw


class Packs:
    def __init__(self, folder):
        self.files = {}
        for name in sorted(os.listdir(folder)):
            if name.lower().endswith(".pk3"):
                z = zipfile.ZipFile(os.path.join(folder, name))
                for n in z.namelist():
                    self.files[n.lower()] = z

    def read(self, path):
        z = self.files.get(path.lower())
        if not z:
            return None
        for n in z.namelist():
            if n.lower() == path.lower():
                return z.read(n)

    def image(self, path):
        base = os.path.splitext(path)[0]
        for ext in (".tga", ".jpg", ".png"):
            d = self.read(base + ext)
            if d:
                return np.asarray(Image.open(io.BytesIO(d)).convert("RGB")).astype(float) / 255
        return np.full((2, 2, 3), 0.5)


def read_md3(data):
    nf, nt, ns = struct.unpack_from("<3i", data, 76)
    otg, osf = struct.unpack_from("<i", data, 96)[0], struct.unpack_from("<i", data, 100)[0]
    tags = []
    for f in range(nf):
        fr = {}
        for t in range(nt):
            o = otg + (f * nt + t) * 112
            v = struct.unpack_from("<12f", data, o + 64)
            fr[data[o:o + 64].split(b"\0")[0].decode().lower()] = (np.array(v[0:3]), np.array(v[3:12]).reshape(3, 3))
        tags.append(fr)
    surfs, o = [], osf
    for _ in range(ns):
        name = data[o + 4:o + 68].split(b"\0")[0].decode().lower()
        snf, _, nv, ntr, otr, osh, ost, oxyz, send = struct.unpack_from("<9i", data, o + 72)
        shader = data[o + osh:o + osh + 64].split(b"\0")[0].decode()
        tris = np.frombuffer(data, "<i4", ntr * 3, o + otr).reshape(-1, 3)
        st = np.frombuffer(data, "<f4", nv * 2, o + ost).reshape(-1, 2)
        raw = np.frombuffer(data, "<i2", nv * 4 * snf, o + oxyz).reshape(snf, nv, 4)
        nrm = raw[:, :, 3].view(np.uint16)
        lat = ((nrm >> 8) & 255) * 2 * math.pi / 255
        lng = (nrm & 255) * 2 * math.pi / 255
        n = np.stack([np.cos(lat) * np.sin(lng), np.sin(lat) * np.sin(lng), np.cos(lng)], -1)
        surfs.append((name, shader, tris, st, raw[:, :, :3] / 64.0, n))
        o += send
    return tags, surfs


def read_skin(text):
    m = {}
    for line in text.decode("latin-1").splitlines():
        if "," in line:
            a, b = line.strip().split(",", 1)
            if b:
                m[a.lower()] = b.strip()
    return m


def idle_frames(cfg):
    nums = []
    for line in cfg.decode("latin-1").splitlines():
        w = line.split("//")[0].split()
        if len(w) >= 4 and w[0].lstrip("-").isdigit():
            nums.append(int(w[0]))
    # Quake III order: 6 deaths, 7 torso, then legs from LEGS_WALKCR; LEGS_IDLE is the 23rd
    skip = nums[13] - nums[6]
    return nums[22] - skip, nums[11]


def split(spec):
    name, _, skin = spec.partition("/")
    return name, skin or "default"


def gather(packs, body, head):
    bname, bskin = split(body)
    hname, hskin = split(head)
    parts = {}
    for part, model, skin in (("lower", bname, bskin), ("upper", bname, bskin), ("head", hname, hskin)):
        md3 = packs.read("models/players/%s/%s.md3" % (model, part))
        sk = packs.read("models/players/%s/%s_%s.skin" % (model, part, skin))
        if not md3 or not sk:
            return None
        parts[part] = read_md3(md3), read_skin(sk)
    legs_f, torso_f = idle_frames(packs.read("models/players/%s/animation.cfg" % bname) or b"")
    meshes = []
    I = (np.zeros(3), np.eye(3))

    def place(parent, tag):
        return (parent[0] + tag[0] @ parent[1], tag[1] @ parent[1])
    (lt, ls), lsk = parts["lower"]
    (ut, us), usk = parts["upper"]
    (ht, hs), hsk = parts["head"]
    torso = place(I, lt[legs_f]["tag_torso"])
    headf = place(torso, ut[torso_f]["tag_head"])
    for (surfs, skin), fr, frame in (((ls, lsk), legs_f, I), ((us, usk), torso_f, torso), ((hs, hsk), 0, headf)):
        for name, shader, tris, st, xyz, n in surfs:
            tex = skin.get(name, shader)
            if not tex or "null" in tex.lower():
                continue
            meshes.append((frame[0] + xyz[fr] @ frame[1], n[fr] @ frame[1], tris, st, packs.image(tex)))
    return meshes


def render(meshes, W, H, yaw=-30, pitch=8):
    img = np.zeros((H, W, 3)) + 0.12
    zb = np.full((H, W), -1e9)
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    light = np.array([0.6, 0.3, 0.75]); light /= np.linalg.norm(light)
    allp = np.concatenate([m[0] for m in meshes])
    zc = (allp[:, 2].max() + allp[:, 2].min()) / 2
    scale = H * 0.9 / (allp[:, 2].max() - allp[:, 2].min())
    for v, nn, tris, st, t in meshes:
        x = v[:, 0] * cy - v[:, 1] * sy
        y = v[:, 0] * sy + v[:, 1] * cy
        depth = x * cp + v[:, 2] * sp
        up = -x * sp + (v[:, 2] - zc) * cp
        sx, syy = W / 2 + y * scale, H / 2 - up * scale
        lit = 0.35 + 0.8 * np.clip(nn @ light, 0, 1)
        th, tw = t.shape[:2]
        for a, b, c in tris:
            xs, ys = np.array([sx[a], sx[b], sx[c]]), np.array([syy[a], syy[b], syy[c]])
            x0, x1 = int(max(0, xs.min())), int(min(W - 1, xs.max() + 1))
            y0, y1 = int(max(0, ys.min())), int(min(H - 1, ys.max() + 1))
            den = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
            if x0 > x1 or y0 > y1 or abs(den) < 1e-9:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            w0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / den
            w1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / den
            w2 = 1 - w0 - w1
            z = w0 * depth[a] + w1 * depth[b] + w2 * depth[c]
            sub = zb[y0:y1 + 1, x0:x1 + 1]
            ok = (w0 >= 0) & (w1 >= 0) & (w2 >= 0) & (z > sub)
            if not ok.any():
                continue
            u = w0 * st[a, 0] + w1 * st[b, 0] + w2 * st[c, 0]
            vv = w0 * st[a, 1] + w1 * st[b, 1] + w2 * st[c, 1]
            col = t[np.floor(vv * th).astype(int) % th, np.floor(u * tw).astype(int) % tw]
            k = w0 * lit[a] + w1 * lit[b] + w2 * lit[c]
            sub[ok] = z[ok]
            img[y0:y1 + 1, x0:x1 + 1][ok] = np.clip(col[ok] * k[ok][:, None], 0, 1)
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("baseq3")
    ap.add_argument("out")
    ap.add_argument("--bodies", nargs="+", required=True)
    ap.add_argument("--heads", nargs="+", required=True)
    ap.add_argument("--size", type=int, default=300)
    ap.add_argument("--yaw", type=float, default=-30)
    a = ap.parse_args()
    packs = Packs(a.baseq3)
    W, H = a.size, int(a.size * 1.3)
    sheet = Image.new("RGB", (W * len(a.heads), (H + 18) * len(a.bodies)), (20, 20, 22))
    draw = ImageDraw.Draw(sheet)
    for r, body in enumerate(a.bodies):
        for c, head in enumerate(a.heads):
            meshes = gather(packs, body, head)
            x, y = c * W, r * (H + 18)
            if meshes:
                tile = render(meshes, W, H, a.yaw)
                sheet.paste(Image.fromarray((tile * 255).astype(np.uint8)), (x, y + 18))
            draw.text((x + 6, y + 3), "%s:%s" % (body, head) if meshes else "%s:%s missing" % (body, head),
                      fill=(230, 230, 230))
    sheet.save(a.out)


if __name__ == "__main__":
    main()
