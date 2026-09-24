# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/qualite_maillage_glb.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# LA QUALITE D'UN MAILLAGE, EN CHIFFRES, AVANT DE DIRE QU'IL EST MAUVAIS (Q14.2).
#
# Rodolf, 22/09 : « le maillage de TripoSR n'est pas du tout au niveau pour
# l'instant. » Cet outil dit EN QUOI, avec des nombres comparables d'une version
# a l'autre : sommets, triangles, composantes connexes, bords (donc trous),
# aretes non-manifold, caracteristique d'Euler, longueurs d'aretes, angles
# diedres, valences.
#
# IL NE DICTE AUCUN ATTENDU. Un attendu recopie (« il faut 5 000 sommets ») se
# perime au premier changement de resolution. Ce qui est juge ici sans attendu,
# ce sont les INVARIANTS : une surface fermee n'a pas de bord, une surface saine
# n'a pas d'arete a trois faces, et V - E + F vaut 2 par composante fermee de
# genre 0. Le reste est DECRIT, pas note.
#
# ET LES SOMMETS SONT SOUDES AVANT TOUTE TOPOLOGIE. Les marching cubes
# produisent des sommets dupliques a la meme position ; compter les composantes
# sans souder donnerait des milliers de morceaux la ou il n'y en a qu'un. La
# tolerance de soudure est RELATIVE a la diagonale de l'objet : un seuil absolu
# mesure l'echelle du fichier autant que sa topologie.
#
# USAGE : python qualite_maillage_glb.py <fichier.glb> [<autre.glb> ...]
# -----------------------------------------------------------------------------
import json
import struct
import sys
from collections import Counter, defaultdict

import numpy as np

for _f in (sys.stdout, sys.stderr):
    try:
        _f.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_TAILLE = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
_NP = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16,
       5125: np.uint32, 5126: np.float32}
_COMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def lire_glb(chemin):
    d = open(chemin, "rb").read()
    if d[:4] != b"glTF":
        raise ValueError("pas un .glb")
    off, js, bin_ = 12, None, b""
    while off < len(d):
        ln, ty = struct.unpack_from("<II", d, off)
        off += 8
        if ty == 0x4E4F534A:
            js = json.loads(d[off:off + ln].decode("utf-8"))
        elif ty == 0x004E4942:
            bin_ = d[off:off + ln]
        off += ln
    return js, bin_


def accesseur(js, bin_, i):
    a = js["accessors"][i]
    bv = js["bufferViews"][a["bufferView"]]
    nc = _COMP[a["type"]]
    unite = _TAILLE[a["componentType"]]
    pas = bv.get("byteStride") or nc * unite
    deb = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    brut = np.frombuffer(bin_, dtype=np.uint8, count=pas * a["count"], offset=deb)
    brut = brut.reshape(a["count"], pas)[:, : nc * unite]
    return np.ascontiguousarray(brut).view(_NP[a["componentType"]]).reshape(a["count"], nc)


def quantiles(x, qs=(50, 90, 99)):
    return ", ".join("p%d=%.4g" % (q, np.percentile(x, q)) for q in qs)


def juger(chemin):
    js, bin_ = lire_glb(chemin)
    prim = js["meshes"][0]["primitives"][0]
    attrs = prim["attributes"]
    P = accesseur(js, bin_, attrs["POSITION"]).astype(np.float64)
    F = accesseur(js, bin_, prim["indices"]).reshape(-1, 3).astype(np.int64)
    print("=" * 78)
    print(chemin)
    print("  attributs      : %s" % ", ".join(sorted(attrs)))
    print("  materiaux      : %s   textures : %s"
          % ("aucun" if not js.get("materials") else len(js["materials"]),
             "aucune" if not js.get("textures") else len(js["textures"])))
    print("  sommets bruts  : %d   triangles : %d   quads : 0 (le glTF ne porte que des triangles)"
          % (len(P), len(F)))
    diag = float(np.linalg.norm(P.max(axis=0) - P.min(axis=0)))
    tol = max(diag * 1e-6, 1e-12)
    cle = np.round(P / tol).astype(np.int64)
    _, inv = np.unique(cle, axis=0, return_inverse=True)
    inv = np.asarray(inv).reshape(-1)
    G = inv[F]
    nsoude = int(inv.max()) + 1
    print("  sommets soudes : %d  (%d doublons de position, soit %.1f %%)"
          % (nsoude, len(P) - nsoude, 100.0 * (len(P) - nsoude) / max(len(P), 1)))
    Pm = np.zeros((nsoude, 3))
    Pm[inv] = P

    ar = np.sort(np.concatenate([G[:, [0, 1]], G[:, [1, 2]], G[:, [2, 0]]]), axis=1)
    uni, cnt = np.unique(ar, axis=0, return_counts=True)
    print("  aretes         : %d   dont BORD (1 face) : %d   NON-MANIFOLD (>2 faces) : %d"
          % (len(uni), int((cnt == 1).sum()), int((cnt > 2).sum())))

    parent = np.arange(nsoude)

    def racine(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for a, b in uni:
        ra, rb = racine(int(a)), racine(int(b))
        if ra != rb:
            parent[rb] = ra
    refs = np.unique(G)
    tailles = sorted(Counter(racine(int(v)) for v in refs).values(), reverse=True)
    print("  composantes    : %d   la plus grosse porte %.1f %% des sommets   (5 premieres : %s)"
          % (len(tailles), 100.0 * tailles[0] / len(refs), tailles[:5]))
    khi = len(refs) - len(uni) + len(F)
    print("  Euler V-E+F    : %d   (2 par composante fermee de genre 0 -> attendu %d ; ecart %d)"
          % (khi, 2 * len(tailles), khi - 2 * len(tailles)))

    L = np.linalg.norm(Pm[uni[:, 0]] - Pm[uni[:, 1]], axis=1)
    print("  arete / diag   : %s   (diagonale %.4g)" % (quantiles(L / diag), diag))

    n = np.cross(Pm[G[:, 1]] - Pm[G[:, 0]], Pm[G[:, 2]] - Pm[G[:, 0]])
    n = n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-20)
    par = defaultdict(list)
    for t in range(len(G)):
        for a, b in ((G[t, 0], G[t, 1]), (G[t, 1], G[t, 2]), (G[t, 2], G[t, 0])):
            par[(min(a, b), max(a, b))].append(t)
    ang = np.asarray([np.degrees(np.arccos(float(np.clip(np.dot(n[f[0]], n[f[1]]), -1.0, 1.0))))
                      for f in par.values() if len(f) == 2])
    if len(ang):
        print("  angle diedre   : %s deg ; aretes > 30 deg : %.2f %% ; > 60 deg : %.2f %%"
              % (quantiles(ang), 100.0 * (ang > 30).mean(), 100.0 * (ang > 60).mean()))

    val = Counter()
    for a, b in uni:
        val[int(a)] += 1
        val[int(b)] += 1
    vv = np.asarray(list(val.values()))
    print("  valence        : moyenne %.2f ; %s ; valence 4 : %.1f %%"
          % (vv.mean(), quantiles(vv), 100.0 * (vv == 4).mean()))


if __name__ == "__main__":
    for c in sys.argv[1:]:
        juger(c)
