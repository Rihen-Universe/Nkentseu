# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
# Deux images pour que Rodolf VOIE le depliage, au lieu de lire des chiffres :
#   1. l'atlas UV -- les 9 ilots, chacun sa couleur, et l'espace qu'ils occupent
#   2. le personnage porte un DAMIER pose par les UV : si les carreaux restent
#      carres et de meme taille partout, le depliage est bon. C'est le seul
#      controle qui ne demande de croire aucun chiffre.

import sys
import numpy as np
from PIL import Image
from collections import defaultdict

OBJ = sys.argv[1]
SORTIE_ATLAS = sys.argv[2]
SORTIE_DAMIER = sys.argv[3]

# ----------------------------------------------------------------- lecture
V, VT, F, FT, G, gcur = [], [], [], [], [], '?'
for ligne in open(OBJ, encoding='utf-8', errors='replace'):
    p = ligne.split()
    if not p:
        continue
    if p[0] == 'v':
        V.append([float(x) for x in p[1:4]])
    elif p[0] == 'vt':
        VT.append([float(x) for x in p[1:3]])
    elif p[0] in ('o', 'g'):
        gcur = p[1] if len(p) > 1 else '?'
    elif p[0] == 'f':
        vs, ts = [], []
        for t in p[1:]:
            m = t.split('/')
            vs.append(int(m[0]) - 1)
            ts.append(int(m[1]) - 1 if len(m) > 1 and m[1] else -1)
        for j in range(1, len(vs) - 1):
            F.append([vs[0], vs[j], vs[j + 1]])
            FT.append([ts[0], ts[j], ts[j + 1]])
            G.append(gcur)
V = np.array(V, dtype=np.float64)
UV = np.array(VT, dtype=np.float64)
F = np.array(F); FT = np.array(FT)
print('%d triangles, %d sommets, %d UV' % (len(F), len(V), len(UV)))

# ilots (composantes connexes UV)
parent = list(range(len(UV)))
def find(a):
    while parent[a] != a:
        parent[a] = parent[parent[a]]; a = parent[a]
    return a
def union(a, b):
    ra, rb = find(a), find(b)
    if ra != rb: parent[ra] = rb
for ts in FT:
    union(ts[0], ts[1]); union(ts[0], ts[2])
racines = {}
ilot = np.empty(len(FT), dtype=np.int32)
for i, ts in enumerate(FT):
    r = find(ts[0])
    if r not in racines:
        racines[r] = len(racines)
    ilot[i] = racines[r]
nI = len(racines)
print('%d ilots' % nI)

PALETTE = np.array([
    [242, 154, 40], [10, 85, 95], [225, 90, 70], [70, 150, 130],
    [150, 100, 190], [230, 190, 60], [90, 130, 210], [200, 110, 150],
    [120, 180, 90], [160, 160, 160]], dtype=np.float64)


def rasteriser(P, res_w, res_h, couleur_fn, z, fond):
    """P : (n,3,2) coordonnees ecran. z : (n,3) profondeur (plus grand = devant)."""
    img = np.tile(np.array(fond, dtype=np.float64), (res_h, res_w, 1))
    zbuf = np.full((res_h, res_w), -1e30)
    for i in range(len(P)):
        tri = P[i]
        x0 = max(0, int(np.floor(tri[:, 0].min())))
        x1 = min(res_w - 1, int(np.ceil(tri[:, 0].max())))
        y0 = max(0, int(np.floor(tri[:, 1].min())))
        y1 = min(res_h - 1, int(np.ceil(tri[:, 1].max())))
        if x1 < x0 or y1 < y0:
            continue
        xs = np.arange(x0, x1 + 1) + 0.5
        ys = np.arange(y0, y1 + 1) + 0.5
        gx, gy = np.meshgrid(xs, ys)
        ax, ay = tri[0]; bx, by = tri[1]; cx, cy = tri[2]
        d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-12:
            continue
        l1 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / d
        l2 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / d
        l3 = 1.0 - l1 - l2
        m = (l1 >= 0) & (l2 >= 0) & (l3 >= 0)
        if not m.any():
            continue
        prof = l1 * z[i][0] + l2 * z[i][1] + l3 * z[i][2]
        sousZ = zbuf[y0:y1 + 1, x0:x1 + 1]
        visible = m & (prof > sousZ)
        if not visible.any():
            continue
        col = couleur_fn(i, l1, l2, l3, visible)
        sousZ[visible] = prof[visible]
        sousI = img[y0:y1 + 1, x0:x1 + 1]
        sousI[visible] = col
    return np.clip(img, 0, 255).astype(np.uint8)


# ============================ 1. L'ATLAS UV ============================
R = 1000
mn = UV.min(axis=0); mx = UV.max(axis=0)
ech = max(1e-12, (mx - mn).max())
q = (UV - mn) / ech
Pa = np.zeros((len(FT), 3, 2))
for i, ts in enumerate(FT):
    for j in range(3):
        Pa[i, j] = [q[ts[j]][0] * (R - 1), (1.0 - q[ts[j]][1]) * (R - 1)]
za = np.zeros((len(FT), 3))

def coul_atlas(i, l1, l2, l3, m):
    base = PALETTE[ilot[i] % len(PALETTE)]
    # un lisere clair sur les bords de triangle : on voit la grille de quads
    bord = np.minimum(np.minimum(l1, l2), l3)[m]
    f = np.clip(bord * 22.0, 0.35, 1.0)[:, None]
    return base[None, :] * f + 255.0 * (1 - f) * 0.30

img = rasteriser(Pa, R, R, coul_atlas, za, [250, 248, 244])
Image.fromarray(img).save(SORTIE_ATLAS)
print('ecrit :', SORTIE_ATLAS)

# ======================= 2. LE PERSONNAGE EN DAMIER =======================
W, H = 900, 1200
c = V.mean(axis=0)
P = V - c
r = np.abs(P).max()
# vue de trois quarts : on tourne un peu pour que les bras se detachent
ang = np.deg2rad(28.0)
ca, sa = np.cos(ang), np.sin(ang)
Rot = np.array([[ca, 0, sa], [0, 1, 0], [-sa, 0, ca]])
P = P @ Rot.T
s = 0.42 * min(W, H) / r
Pp = np.zeros((len(F), 3, 2))
zz = np.zeros((len(F), 3))
for i, vs in enumerate(F):
    for j in range(3):
        p = P[vs[j]]
        Pp[i, j] = [W * 0.5 + p[0] * s, H * 0.5 - p[1] * s]
        zz[i, j] = p[2]
# normale par face, dans le repere tourne
N = np.cross(P[F[:, 1]] - P[F[:, 0]], P[F[:, 2]] - P[F[:, 0]])
nn = np.linalg.norm(N, axis=1, keepdims=True)
N = N / np.maximum(nn, 1e-12)
L = np.array([0.35, 0.55, 0.75]); L = L / np.linalg.norm(L)
lam = np.clip(N @ L, 0, 1)

CASES = 28.0  # nombre de carreaux sur la largeur de l'atlas

def coul_damier(i, l1, l2, l3, m):
    ts = FT[i]
    u = (l1 * UV[ts[0]][0] + l2 * UV[ts[1]][0] + l3 * UV[ts[2]][0])[m]
    v = (l1 * UV[ts[0]][1] + l2 * UV[ts[1]][1] + l3 * UV[ts[2]][1])[m]
    d = ((np.floor(u * CASES) + np.floor(v * CASES)) % 2)
    a = np.array([242, 154, 40], dtype=np.float64)   # orange Rihen
    b = np.array([10, 85, 95], dtype=np.float64)     # petrole Rihen
    base = d[:, None] * a[None, :] + (1 - d[:, None]) * b[None, :]
    ecl = (0.30 + 0.70 * lam[i])
    return base * ecl

img2 = rasteriser(Pp, W, H, coul_damier, zz, [250, 248, 244])
Image.fromarray(img2).save(SORTIE_DAMIER)
print('ecrit :', SORTIE_DAMIER)
