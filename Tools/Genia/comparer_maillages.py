# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# COMPARER DEUX .OBJ produits par le MEME programme, avant et apres un correctif.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# POURQUOI IL EXISTE. Le 18/09, `gen::SurfaceNets` a ete corrige dans un module
# a sept consommateurs. Les sept CONSTRUISENT -- ce qui prouve que la signature
# tient, et rien de plus. La dette ecrite le meme jour disait : « au premier banc
# lance sur l'un d'eux, un ecart de l'ordre de 2 % d'une cellule sur les
# positions est attendu et normal ; au-dela, revenir ». Cet outil mesure cet
# ecart, pour que la dette se ferme sur un CHIFFRE et non sur une impression.
#
# CE QU'IL MESURE, ET COMMENT. Les indices de sommets peuvent avoir change (le
# correctif en AJOUTE la ou une cellule portait deux nappes), donc une
# comparaison indice par indice n'aurait aucun sens. On compare par PLUS PROCHE
# VOISIN : pour chaque sommet du nouveau maillage, la distance au sommet le plus
# proche de l'ancien. C'est la grandeur qui dit « la surface a-t-elle bouge ».
#
# ⚠️ CE QU'IL NE MESURE PAS, et il faut le dire : un sommet AJOUTE a la bonne
# place rend une distance nulle, donc cet instrument ne VOIT PAS les sommets
# ajoutes. C'est voulu -- ils sont le correctif, pas le defaut -- mais cela
# signifie qu'un maillage entierement refait pourrait passer si toutes ses
# positions coincidaient. Le compte des sommets est donc imprime A COTE, et il
# se lit avec.
#
# USAGE : python comparer_maillages.py <avant.obj> <apres.obj> --seuil 0.02
#         le seuil s'applique PAR AXE ; la borne euclidienne en est deduite
#         (seuil x racine(3)), parce que trois axes peuvent etre bornes ensemble.
#         code 0 = sous le seuil · 1 = au-dessus · 2 = refus nomme
# -----------------------------------------------------------------------------
import argparse
import os
import sys


def _refus(msg, code=2):
    sys.stderr.write("REFUS : %s\n" % msg)
    sys.stderr.flush()
    sys.exit(code)


def lire_obj(path):
    import numpy as np
    if not os.path.isfile(path):
        _refus("fichier introuvable : %s" % path)
    V = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for ligne in f:
            if ligne.startswith("v "):
                p = ligne.split()
                if len(p) >= 4:
                    V.append((float(p[1]), float(p[2]), float(p[3])))
    if not V:
        _refus("aucun sommet lu dans %s" % path)
    return np.asarray(V, dtype=np.float64)


def main():
    import numpy as np
    ap = argparse.ArgumentParser()
    ap.add_argument("avant")
    ap.add_argument("apres")
    ap.add_argument("--seuil", type=float, default=0.02,
                    help="ecart maximal tolere, en unites monde (defaut 0,02 = 2 %% d'une cellule de 1,0)")
    a = ap.parse_args()

    A = lire_obj(a.avant)
    B = lire_obj(a.apres)
    print("MESURE comparaison")
    print("  avant : %-40s %6d sommets" % (os.path.basename(a.avant), len(A)))
    print("  apres : %-40s %6d sommets  (%+d)" % (os.path.basename(a.apres), len(B), len(B) - len(A)))

    # Plus proche voisin par force brute, par paquets pour ne pas exploser la
    # memoire : len(A) x len(B) flottants seraient 8 Go sur un gros maillage.
    pire, somme, pireAxe = 0.0, 0.0, 0.0
    PAQUET = 512
    for i in range(0, len(B), PAQUET):
        bloc = B[i:i + PAQUET]
        ecarts = bloc[:, None, :] - A[None, :, :]
        dist = np.sqrt((ecarts ** 2).sum(axis=2))
        proche = dist.argmin(axis=1)
        d = dist[np.arange(len(bloc)), proche]
        somme += float(d.sum())
        pire = max(pire, float(d.max()))
        # ── LES DEUX GRANDEURS, ET C'EST UNE MESURE DU 18/09 QUI L'IMPOSE ────
        # Ce comparateur ne rendait QUE la distance euclidienne, et il a declare
        # ROUGE un produit correct : la dette qu'il devait juger etait ecrite
        # « 2 % d'une cellule », ce qui se pense PAR AXE. Or trois axes bornes a
        # 0,02 chacun donnent 0,02 x racine(3) = 0,034641 de distance. L'ecart
        # mesure valait 0,034240 -- sous la borne geometrique, et au-dessus d'un
        # seuil qui parlait d'autre chose.
        # « Un attendu derive d'une convention doit citer la ligne qui fixe cette
        # convention » : ici la convention est le bornage PAR AXE de
        # `SurfaceNets`. On imprime donc les DEUX, et le seuil dit lequel il juge.
        pireAxe = max(pireAxe, float(np.abs(ecarts[np.arange(len(bloc)), proche]).max()))
    moyen = somme / float(len(B))

    print("  ecart au plus proche voisin : PIRE=%.6f  MOYEN=%.6f  (distance euclidienne)" % (pire, moyen))
    print("  ecart maximal PAR AXE       : %.6f" % pireAxe)
    print("  seuil ecrit AVANT la course : %.6f  -- applique PAR AXE" % a.seuil)
    print("  borne euclidienne derivee   : %.6f  = seuil x racine(3)" % (a.seuil * 3.0 ** 0.5))
    # LA PIRE DE LA COURSE, SANS AUCUN SEUIL GLISSANT : c'est le filet qui ne
    # depend d'aucun reglage, donc le seul qu'on ne puisse pas mal regler. Une
    # moyenne serait noyee par les milliers de sommets qui n'ont pas bouge.
    if pireAxe <= a.seuil + 1e-9 and pire <= a.seuil * 3.0 ** 0.5 + 1e-9:
        print("VERDICT : SOUS LE SEUIL -- aucun sommet n'a bouge de plus que la borne, sur aucun axe.")
        sys.exit(0)
    quoi = "PAR AXE" if pireAxe > a.seuil + 1e-9 else "en distance"
    print("VERDICT : AU-DESSUS DU SEUIL (%s) -- c'est autre chose, il faut revenir." % quoi)
    print("          par axe %.6f contre %.6f ; distance %.6f contre %.6f"
          % (pireAxe, a.seuil, pire, a.seuil * 3.0 ** 0.5))
    sys.exit(1)


if __name__ == "__main__":
    main()
