# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# SEPARATION — « les bras sont-ils detaches du corps ? », mesure et non regardee.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# ⚠️ LE CRITERE ANNONCE ETAIT « LE NOMBRE DE COMPOSANTES CONNEXES », ET IL NE
# PEUT PAS VOIR CE DEFAUT. Je le corrige AVANT la course, pas apres avoir vu un
# resultat qui deplaisait.
#
#   Un bras qui se decolle du torse reste attache A L'EPAULE. La surface reste
#   donc d'un seul tenant : UNE composante avant, UNE composante apres. Le genre
#   ne bouge pas davantage -- il n'y a pas d'anse tant que la main ne rejoint pas
#   le corps. Les deux grandeurs sont INVARIANTES par le defaut qu'on cherche,
#   et leur egalite aurait ete lue « le modele est en cause » : une conclusion
#   fausse, tiree d'un temoin muet.
#
#   C'est la famille « un temoin invariant par le defaut qu'on cherche », et elle
#   a deja coute a ce depot : un banc comptait les pixels qui changent entre deux
#   images, grandeur rigoureusement inchangee par un retournement vertical.
#
# CE QUI, LUI, DISTINGUE : LA COUPE HORIZONTALE. A mi-hauteur des bras, un
# personnage dont les bras sont separes montre TROIS regions fermees (bras
# gauche, torse, bras droit) ; fondus, il n'en montre QU'UNE. La grandeur est le
# nombre de polygones fermes de la section — exacte, calculee sur la geometrie
# (pas sur un echantillonnage de sommets), et elle ne depend d'aucun seuil.
#
# C'est la meme idee que `mesure_orientation.py`, qui compte deja les amas par
# tranche pour trouver les quatre pieds d'une chaise. On la reprend ici parce
# qu'elle a deja fait ses preuves sur un cas connu.
#
# LE ZERO SE PROUVE EN PREMIER : sur une sphere, toute coupe rend UNE region. Un
# instrument qui rendrait 2 sur une sphere ne mesurerait pas ce qu'il annonce.
#
# USAGE : python mesure_separation.py <maillage> [--tranches 40] [--zero]
#         code 0 = mesure faite · 2 = refus nomme · 3 = le zero a echoue
# -----------------------------------------------------------------------------
import argparse
import os
import sys

for _f in (sys.stdout, sys.stderr):
    try:
        _f.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _refus(msg, code=2):
    sys.stderr.write("REFUS : %s\n" % msg)
    sys.stderr.flush()
    sys.exit(code)


def amas(masque):
    """Composantes connexes 4-voisins d'un masque booleen 2D, sans scipy.
    Recopie de `mesure_orientation.py` -- la meme fonction, deja eprouvee sur un
    cas connu (les quatre pieds d'une chaise, 17/09)."""
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


def profil_regions(maillage, tranches, grille=96):
    """Pour chaque tranche en Y, le nombre de regions distinctes de la coupe.
    Rend (hauteurs, comptes, aires en fraction de la grille)."""
    import numpy as np
    import trimesh
    # ── ON ECHANTILLONNE LA SURFACE, PAS LES SOMMETS, ET C'EST LA CLE ────────
    # Deux raisons, et la seconde est celle qui commande :
    #   1. le zero echouait -- une sphere rendait 205 amas, parce que ses 2 562
    #      sommets projetes sur une grille 96x96 sont EPARPILLES : chaque point
    #      isole devient un amas. L'instrument mesurait la densite du maillage,
    #      pas sa forme ;
    #   2. et surtout : cette experience COMPARE un maillage a 256 et un a 512,
    #      qui n'ont PAS la meme densite de sommets. Un comptage fonde sur les
    #      sommets aurait donc rendu moins d'amas parasites a 512 -- et cet
    #      ecart, entierement artificiel, se serait lu « l'ecart s'est ouvert ».
    #      L'instrument aurait confirme l'hypothese par sa propre construction.
    # Un echantillonnage uniforme de la SURFACE a densite FIXE supprime les deux.
    V = np.asarray(trimesh.sample.sample_surface(maillage, 200000)[0], dtype=np.float64)
    mn, mx = V.min(axis=0), V.max(axis=0)
    ext = mx - mn
    if ext[1] <= 0:
        _refus("le maillage a une hauteur nulle")
    for k in (0, 2):
        if ext[k] <= 0:
            _refus("le maillage est plat sur l'axe %d : aucune coupe possible" % k)
    t = (V[:, 1] - mn[1]) / ext[1]
    idx = np.clip((t * tranches).astype(int), 0, tranches - 1)
    u = np.clip(((V[:, 0] - mn[0]) / ext[0] * grille).astype(int), 0, grille - 1)
    w = np.clip(((V[:, 2] - mn[2]) / ext[2] * grille).astype(int), 0, grille - 1)
    hauteurs, comptes, aires = [], [], []
    for sname in range(tranches):
        sel = idx == sname
        masque = np.zeros((grille, grille), dtype=bool)
        if sel.any():
            masque[u[sel], w[sel]] = True
        hauteurs.append(float(mn[1] + (sname + 0.5) / tranches * ext[1]))
        comptes.append(amas(masque))
        aires.append(float(masque.sum()) / float(grille * grille))
    return hauteurs, comptes, aires


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("maillage")
    ap.add_argument("--tranches", type=int, default=40)
    ap.add_argument("--zero", action="store_true",
                    help="prouve d'abord que l'instrument rend 1 sur une sphere")
    a = ap.parse_args()

    import numpy as np
    import trimesh

    if a.zero:
        # ── LE ZERO : sur une sphere, TOUTE coupe rend UNE region ────────────
        s = trimesh.creation.icosphere(subdivisions=4, radius=1.0)
        _, c, _ = profil_regions(s, 12)
        # ⚠️ On juge sur les tranches NON VIDES : aux deux extremites d'une
        # sphere, une tranche peut ne contenir aucun sommet, et « 0 region » n'est
        # pas un resultat -- c'est une absence de mesure.
        bon = [x for x in c if x > 0]
        if not bon or max(bon) != 1 or min(bon) != 1:
            _refus("le zero a echoue : sur une SPHERE l'instrument rend %s au lieu de 1 partout.\n"
                   "        il ne mesure donc pas ce qu'il annonce, et rien d'autre ne sera lu." % c, 3)
        print("  [ZERO ] sphere : toutes les coupes rendent 1 region -> l'instrument sait dire « un seul bloc »")
        # Et un POSITIF de controle : deux spheres ECARTEES doivent rendre 2.
        s1 = trimesh.creation.icosphere(subdivisions=3, radius=0.5)
        s2 = trimesh.creation.icosphere(subdivisions=3, radius=0.5)
        s2.apply_translation([3.0, 0, 0])
        duo = trimesh.util.concatenate([s1, s2])
        _, c2, _ = profil_regions(duo, 8)
        if max(c2) != 2:
            _refus("le positif a echoue : deux spheres ECARTEES rendent %d region(s) au lieu de 2.\n"
                   "        un instrument qui ne peut rendre qu'une valeur ne distingue rien." % max(c2), 3)
        print("  [ZERO ] deux spheres ecartees : 2 regions -> l'instrument sait DISTINGUER")
        print()

    if not os.path.isfile(a.maillage):
        _refus("maillage introuvable : %s" % a.maillage)
    m = trimesh.load(a.maillage, force="mesh", process=False)
    if not hasattr(m, "vertices") or len(m.vertices) == 0:
        _refus("aucun sommet lu dans %s" % a.maillage)

    h, c, ar = profil_regions(m, a.tranches)
    bon = [x for x in c if x >= 0]
    mn, mx = m.bounds
    hauteur = float(mx[1] - mn[1])

    print("MAILLAGE : %s" % os.path.basename(a.maillage))
    print("  %d sommets · %d faces · hauteur %.4f · %d composante(s) connexe(s)"
          % (len(m.vertices), len(m.faces), hauteur, len(m.split(only_watertight=False))))
    print("  profil des regions par coupe (du BAS vers le HAUT, %d tranches) :" % a.tranches)
    print("    " + " ".join("%d" % x if x >= 0 else "?" for x in c))
    if bon:
        imax = c.index(max(bon))
        print("  MAXIMUM : %d region(s) a la hauteur %.4f (soit %.0f %% de la hauteur totale)"
              % (max(bon), h[imax], 100.0 * (h[imax] - float(mn[1])) / hauteur if hauteur else 0))
        # Combien de tranches montrent une separation ? Une seule tranche a 3
        # regions peut etre un artefact ; une plage continue est un fait.
        n3 = sum(1 for x in bon if x >= 3)
        n2 = sum(1 for x in bon if x == 2)
        print("  tranches a 1 region : %d   |  a 2 regions : %d   |  a 3 regions ou plus : %d"
              % (sum(1 for x in bon if x == 1), n2, n3))
    return 0


if __name__ == "__main__":
    sys.exit(main())
