# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
#
# LE CRITERE QUE LA DISTORSION NE PEUT PAS VOIR : LE RECOUVREMENT DES ILOTS.
#
# POURQUOI IL EXISTE. `mesurer_uv_obj.py` juge chaque face ISOLEMENT (sa forme
# comparee a sa forme 3D). Il ne regarde JAMAIS deux faces ensemble. Donc un
# atlas dont les 9 ilots occupent EXACTEMENT le meme carre affiche une
# distorsion parfaite et une couture nulle : tous les compteurs sont verts et
# le maillage est inutilisable -- peindre l'oeil peindrait aussi le genou.
# C'est la famille « critere invariant par le defaut cherche ».
#
# CE QU'IL MESURE :
#   [A] RECOUVREMENT INTER-ILOTS : un texel touche par DEUX ilots distincts.
#       L'attendu d'un atlas correct est ZERO. Pas « peu » : zero.
#   [B] OCCUPATION : part des texels de l'atlas effectivement couverts.
#       Elle dit si l'empaquetage est serre ou gaspilleur.
#
# ⚠️ CE QU'IL NE MESURE PAS, ET IL FAUT LE DIRE : le REPLI d'un ilot sur
# lui-meme. Les faces voisines d'un meme ilot partagent legitimement les texels
# de leur arete commune, donc compter les multiplicites a l'interieur d'un ilot
# confondrait un repli avec un bord partage. Un ilot replie passerait ici.
#
# SA PREUVE, DANS CET ORDRE (« prouver le zero d'abord ») :
#   1. ZERO   : on repartit les ilots sur une grille -- le recouvrement DOIT
#               tomber a 0 exactement. Un critere qui ne sait pas dire zero ne
#               sert a rien pour juger un futur empaquetage.
#   2. NEGATIF: on superpose volontairement deux ilots -- il DOIT rougir.
#   3. REEL   : on mesure le fichier tel qu'il est.

import sys
import numpy as np
from collections import defaultdict

RES = 512  # texels par cote


def charger(chemin):
    V, VT, F, FT = [], [], [], []
    for ligne in open(chemin, encoding='utf-8', errors='replace'):
        p = ligne.split()
        if not p:
            continue
        if p[0] == 'v':
            V.append([float(x) for x in p[1:4]])
        elif p[0] == 'vt':
            VT.append([float(x) for x in p[1:3]])
        elif p[0] == 'f':
            vs, ts = [], []
            for t in p[1:]:
                m = t.split('/')
                vs.append(int(m[0]) - 1)
                ts.append(int(m[1]) - 1 if len(m) > 1 and m[1] else -1)
            F.append(vs); FT.append(ts)
    return np.array(V), np.array(VT), F, FT


def ilots(uv, FT):
    """Composantes connexes dans le plan UV : un ilot = des faces qui partagent
    des indices UV."""
    parent = list(range(len(uv)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]; a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    for ts in FT:
        for j in range(1, len(ts)):
            union(ts[0], ts[j])
    racines, num = {}, []
    for ts in FT:
        r = find(ts[0])
        if r not in racines:
            racines[r] = len(racines)
        num.append(racines[r])
    return np.array(num), len(racines)


def rasteriser(uv, FT, ilot_de_face, res=RES):
    """Rend chaque triangle dans une grille de texels. Retourne :
       - premier : id de l'ilot ayant marque chaque texel (-1 si vide)
       - recouvre : booleen, texel touche par >= 2 ilots DIFFERENTS"""
    mn = uv.min(axis=0); mx = uv.max(axis=0)
    etendue = np.maximum(mx - mn, 1e-12)
    q = (uv - mn) / etendue.max()  # meme echelle sur les deux axes
    premier = np.full((res, res), -1, dtype=np.int32)
    recouvre = np.zeros((res, res), dtype=bool)
    for ts, isl in zip(FT, ilot_de_face):
        for j in range(1, len(ts) - 1):
            tri = np.array([q[ts[0]], q[ts[j]], q[ts[j + 1]]]) * (res - 1)
            x0 = max(0, int(np.floor(tri[:, 0].min())))
            x1 = min(res - 1, int(np.ceil(tri[:, 0].max())))
            y0 = max(0, int(np.floor(tri[:, 1].min())))
            y1 = min(res - 1, int(np.ceil(tri[:, 1].max())))
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
            dedans = (l1 >= 0) & (l2 >= 0) & (l3 >= 0)
            if not dedans.any():
                continue
            sousP = premier[y0:y1 + 1, x0:x1 + 1]
            sousR = recouvre[y0:y1 + 1, x0:x1 + 1]
            # conflit = deja marque par un AUTRE ilot
            conflit = dedans & (sousP >= 0) & (sousP != isl)
            sousR |= conflit
            vide = dedans & (sousP < 0)
            sousP[vide] = isl
    return premier, recouvre


def juger(uv, FT, ilot_de_face, nIlots, etiquette):
    premier, recouvre = rasteriser(uv, FT, ilot_de_face)
    occupes = int((premier >= 0).sum())
    chevauches = int(recouvre.sum())
    total = premier.size
    pct = 100.0 * chevauches / max(1, occupes)
    print('  %-34s ilots %3d | occupation %5.1f %% | RECOUVREMENT %6d texels (%5.1f %% des occupes) -> %s'
          % (etiquette, nIlots, 100.0 * occupes / total, chevauches, pct,
             'VERT' if chevauches == 0 else 'ROUGE'))
    return chevauches, occupes


def main():
    chemin = sys.argv[1]
    V, uv, F, FT = charger(chemin)
    if len(uv) == 0:
        print('AUCUNE UV'); return
    ilot_de_face, n = ilots(uv, FT)
    print(chemin.replace('\\', '/').split('/')[-1],
          ': %d faces, %d UV, %d ilots' % (len(F), len(uv), n))
    print('')

    # --- 1. LE ZERO D'ABORD : on repartit les ilots sur une grille ----------
    # Si le critere ne sait pas rendre ZERO sur un atlas correctement etale,
    # il ne pourra jamais juger un empaquetage.
    cote = int(np.ceil(np.sqrt(n)))
    uvz = uv.copy()
    for i in range(n):
        m = np.zeros(len(uv), dtype=bool)
        for ts, isl in zip(FT, ilot_de_face):
            if isl == i:
                for t in ts:
                    m[t] = True
        if not m.any():
            continue
        sub = uv[m]
        mn = sub.min(axis=0); mx = sub.max(axis=0)
        ech = max(1e-12, (mx - mn).max())
        # chaque ilot ramene dans sa case, avec une marge de 4 %
        uvz[m] = (uv[m] - mn) / ech * 0.92 + np.array([i % cote, i // cote]) * 1.0
    print('  -- 1. LE ZERO (les ilots repartis sur une grille %dx%d) --' % (cote, cote))
    z, _ = juger(uvz, FT, ilot_de_face, n, 'atlas etale')

    # --- 2. LE NEGATIF : on superpose volontairement deux ilots -------------
    print('  -- 2. LE NEGATIF (deux ilots ramenes sur la meme case) --')
    uvn = uvz.copy()
    if n >= 2:
        m1 = np.zeros(len(uv), dtype=bool)
        for ts, isl in zip(FT, ilot_de_face):
            if isl == 1:
                for t in ts:
                    m1[t] = True
        m0 = np.zeros(len(uv), dtype=bool)
        for ts, isl in zip(FT, ilot_de_face):
            if isl == 0:
                for t in ts:
                    m0[t] = True
        if m1.any() and m0.any():
            uvn[m1] = uvn[m1] - uvz[m1].min(axis=0) + uvz[m0].min(axis=0)
    ng, _ = juger(uvn, FT, ilot_de_face, n, 'ilot 1 pose sur ilot 0')

    # --- 3. LE FICHIER REEL ------------------------------------------------
    print('  -- 3. LE FICHIER TEL QU IL EST --')
    r, _ = juger(uv, FT, ilot_de_face, n, 'atlas livre')

    print('')
    print('  === VERDICT DE L INSTRUMENT ===')
    ok_zero = (z == 0)
    ok_neg = (ng > 0)
    print('   le zero est atteint (atlas etale -> 0)      : %s' % ('OUI' if ok_zero else 'NON *** INSTRUMENT INUTILISABLE ***'))
    print('   le negatif rougit (deux ilots superposes)   : %s' % ('OUI' if ok_neg else 'NON *** CRITERE AVEUGLE ***'))
    if ok_zero and ok_neg:
        print('   -> l instrument sait dire ZERO et sait REFUSER. Son verdict sur le')
        print('      fichier livre est donc recevable.')


if __name__ == '__main__':
    main()
