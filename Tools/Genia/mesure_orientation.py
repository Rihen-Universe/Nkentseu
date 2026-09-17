# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# MESURE D'ORIENTATION -- quel axe est le VERTICAL d'un objet, et dans quel sens.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# POURQUOI IL EXISTE. L'apercu du 17/09 montre la chaise produite par TripoSR
# les pieds EN L'AIR. Mon rasteriseur suppose Y vers le haut (il tourne autour
# de Y et remonte +Y a l'ecran) : si l'objet est droit dans un repere Y-up,
# l'image doit le montrer droit. Mais un oeil n'est pas une mesure, et « ca a
# l'air renverse » ne se commite pas. Il faut un critere qui tranche SEUL.
#
# LE CRITERE, ET IL EST DERIVE DE L'OBJET, PAS D'UNE CONVENTION.
#   Une chaise a QUATRE PIEDS separes d'un cote et UNE assise pleine de l'autre.
#   Donc : on tranche le maillage perpendiculairement a chaque axe, et on compte
#   les COMPOSANTES CONNEXES de chaque section. Du cote des pieds, une section
#   rend ~4 amas ; du cote de l'assise, 1 seul. Cela donne l'AXE (celui dont les
#   sections varient) ET LE SENS (les pieds sont du cote a 4 amas), sans jamais
#   supposer ce que TripoSR ecrit.
#
# LE SECOND INSTRUMENT, SANS CODE COMMUN AVEC LE PREMIER.
#   L'AIRE couverte par la section (fraction de cellules occupees). Les pieds
#   sont fins, l'assise est large. Il doit designer le MEME axe et le MEME sens.
#   S'il designe autre chose, aucun des deux ne conclut -- on le dit.
#
# CE QU'IL REFUSE DE FAIRE. Sur une sphere ou un cube, TOUTES les sections ont
# une seule composante : le critere ne distingue rien, et il doit alors se TAIRE
# (« condition non remplie »), pas inventer un axe. Un temoin qui se prononce
# hors de sa condition de validite fabrique du bruit.
#
# USAGE : python mesure_orientation.py <maillage> [--tranches 24] [--grille 64]
#         code 0 = un verdict est rendu · 3 = condition non remplie · 2 = refus
# -----------------------------------------------------------------------------
import argparse
import os
import sys


def _refus(msg, code=2):
    sys.stderr.write("REFUS : %s\n" % msg)
    sys.stderr.flush()
    sys.exit(code)


def charger(path):
    import numpy as np
    import trimesh
    if not os.path.isfile(path):
        _refus("maillage introuvable : %s" % path)
    m = trimesh.load(path, force="mesh", process=False)
    if not hasattr(m, "vertices") or len(m.vertices) == 0:
        _refus("aucun sommet lu dans %s" % path)
    return np.asarray(m.vertices, dtype=np.float64)


def amas(masque):
    """Composantes connexes 4-voisins d'un masque booleen 2D, sans scipy."""
    import numpy as np
    h, w = masque.shape
    vu = np.zeros((h, w), dtype=bool)
    n = 0
    for i0 in range(h):
        for j0 in range(w):
            if not masque[i0, j0] or vu[i0, j0]:
                continue
            n += 1
            pile = [(i0, j0)]
            vu[i0, j0] = True
            while pile:
                i, j = pile.pop()
                for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    a, b = i + di, j + dj
                    if 0 <= a < h and 0 <= b < w and masque[a, b] and not vu[a, b]:
                        vu[a, b] = True
                        pile.append((a, b))
    return n


def profil(V, axe, nTranches, grille):
    """Pour chaque tranche le long de `axe` : nombre d'amas et fraction d'aire."""
    import numpy as np
    autres = [k for k in (0, 1, 2) if k != axe]
    mn, mx = V.min(axis=0), V.max(axis=0)
    etendue = mx - mn
    for k in autres + [axe]:
        if etendue[k] <= 0.0:
            _refus("l'objet est plat sur l'axe %d : aucune tranche possible" % k)
    t = (V[:, axe] - mn[axe]) / etendue[axe]
    idx = np.clip((t * nTranches).astype(int), 0, nTranches - 1)
    u = np.clip(((V[:, autres[0]] - mn[autres[0]]) / etendue[autres[0]] * grille).astype(int), 0, grille - 1)
    v = np.clip(((V[:, autres[1]] - mn[autres[1]]) / etendue[autres[1]] * grille).astype(int), 0, grille - 1)
    nb, aire = [], []
    for s in range(nTranches):
        sel = idx == s
        masque = np.zeros((grille, grille), dtype=bool)
        if sel.any():
            masque[u[sel], v[sel]] = True
        nb.append(amas(masque))
        aire.append(float(masque.sum()) / float(grille * grille))
    return nb, aire


def main():
    import numpy as np
    ap = argparse.ArgumentParser()
    ap.add_argument("maillage")
    ap.add_argument("--tranches", type=int, default=24)
    ap.add_argument("--grille", type=int, default=64)
    a = ap.parse_args()

    V = charger(a.maillage)
    print("MESURE orientation : %s" % os.path.abspath(a.maillage))
    print("  sommets=%d  boite min=(%.4f %.4f %.4f) max=(%.4f %.4f %.4f)"
          % (len(V), V[:, 0].min(), V[:, 1].min(), V[:, 2].min(),
             V[:, 0].max(), V[:, 1].max(), V[:, 2].max()))

    noms = ("X", "Y", "Z")
    # Les deux quarts EXTREMES de chaque axe, compares entre eux. Le quart, et
    # non la tranche unique : une tranche isolee est bruitee par la resolution.
    verdicts = {}
    for axe in (0, 1, 2):
        nb, aire = profil(V, axe, a.tranches, a.grille)
        q = max(1, a.tranches // 4)
        basAmas, hautAmas = float(np.mean(nb[:q])), float(np.mean(nb[-q:]))
        basAire, hautAire = float(np.mean(aire[:q])), float(np.mean(aire[-q:]))
        verdicts[axe] = (basAmas, hautAmas, basAire, hautAire)
        print("  axe %s : amas bas=%.2f haut=%.2f | aire bas=%.3f haut=%.3f"
              % (noms[axe], basAmas, hautAmas, basAire, hautAire))
        print("           profil amas = %s" % " ".join(str(x) for x in nb))

    # INSTRUMENT [I] : l'axe dont les extremites different le PLUS en nombre
    # d'amas. Condition de validite : un cote doit porter au moins 2 amas en
    # moyenne, sinon rien n'est separe et le critere ne distingue rien.
    ecarts = {k: abs(v[0] - v[1]) for k, v in verdicts.items()}
    axeI = max(ecarts, key=ecarts.get)
    bI, hI = verdicts[axeI][0], verdicts[axeI][1]
    if max(bI, hI) < 2.0 or ecarts[axeI] < 1.0:
        print("VERDICT : CONDITION NON REMPLIE -- aucune extremite ne se separe en")
        print("          plusieurs amas (max=%.2f sur l'axe %s). Cet objet n'a pas de" % (max(bI, hI), noms[axeI]))
        print("          pieds : le critere se TAIT plutot que d'inventer un axe.")
        sys.exit(3)
    sensI = "-" if bI > hI else "+"   # les pieds sont du cote a PLUS d'amas
    print("  [I]  amas     : axe %s, pieds du cote %s%s" % (noms[axeI], sensI, noms[axeI]))

    # INSTRUMENT [II] : l'AIRE. Aucune ligne commune avec le comptage d'amas.
    # Les pieds sont fins -> petite aire ; l'assise est large -> grande aire.
    ratios = {}
    for k, v in verdicts.items():
        lo, hi = min(v[2], v[3]), max(v[2], v[3])
        ratios[k] = hi / lo if lo > 0.0 else 0.0
    axeII = max(ratios, key=ratios.get)
    sensII = "-" if verdicts[axeII][2] < verdicts[axeII][3] else "+"
    print("  [II] aire     : axe %s, pieds du cote %s%s (rapport %.2f)"
          % (noms[axeII], sensII, noms[axeII], ratios[axeII]))

    if axeI != axeII or sensI != sensII:
        print("VERDICT : LES DEUX INSTRUMENTS NE S'ACCORDENT PAS (%s%s contre %s%s)."
              % (sensI, noms[axeI], sensII, noms[axeII]))
        print("          Aucun des deux ne conclut seul. Dit, pas cache.")
        sys.exit(3)

    # L'ATTENDU, DERIVE : dans un repere Y vers le haut, les pieds d'une chaise
    # sont du cote -Y. Toute autre reponse dit que l'objet N'EST PAS droit.
    droit = (axeI == 1 and sensI == "-")
    print("  attendu (repere Y vers le haut, objet DROIT) : pieds du cote -Y")
    print("  mesure                                       : pieds du cote %s%s" % (sensI, noms[axeI]))
    if droit:
        print("VERDICT : L'OBJET EST DROIT dans un repere Y vers le haut.")
    elif axeI == 1 and sensI == "+":
        print("VERDICT : L'OBJET EST RENVERSE -- meme axe, sens OPPOSE (rotation de 180")
        print("          degres autour de X ou Z). Ce n'est pas un defaut de maillage :")
        print("          c'est une CONVENTION d'axe, et elle se corrige par une matrice.")
    else:
        print("VERDICT : L'AXE VERTICAL EST %s, pas Y. Le repere n'est pas Y vers le haut." % noms[axeI])
    sys.exit(0)


if __name__ == "__main__":
    main()
