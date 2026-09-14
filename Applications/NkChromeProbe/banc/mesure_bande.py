#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""mesure_bande.py — compte la bande de couleur CIBLE dans une capture.

AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

Mesure (b1) du canal chrome. Ce script ne sait RIEN du shell : il compte des
pixels d'une couleur exacte, ligne par ligne, et rend la plus grande bande
CONTIGUE de lignes dont le taux de remplissage depasse un seuil.

POURQUOI L'EGALITE EXACTE ET PAS UNE TOLERANCE
  La sonde peint le fond de la barre en (255,0,0), une valeur qu'aucun jeton du
  theme par defaut ne porte. Une tolerance rapprocherait `danger` (232,106,106)
  de la cible et fabriquerait des lignes qui n'existent pas. Ici, un pixel
  compte ou ne compte pas.

POURQUOI ON RAPPORTE LE BORD D'ANCRAGE
  Une capture peut sortir RETOURNEE selon le dorsal (defaut connu du depot). On
  ne corrige pas en silence : on dit a quel bord la bande touche. Une bande qui
  touche le bas au lieu du haut est un retournement, pas une mauvaise hauteur.
"""

import sys
from PIL import Image

CIBLE = (255, 0, 0)
SEUIL = 0.80  # part de la largeur qui doit etre exactement CIBLE


def mesure(chemin, seuil_presence=0.02):
    """Rend l'ETENDUE de la cible : de la premiere a la derniere ligne qui en porte.

    ⚠️ POURQUOI L'ETENDUE ET NON « LA PLUS LONGUE BANDE CONTIGUE AU-DESSUS D'UN
       SEUIL » — c'est une correction, et elle vient d'une mesure, pas d'un avis.
       Mon premier estimateur exigeait 80 % de la largeur ; le PROFIL montre que
       les lignes du milieu de la barre tombent a 0,69-0,79 a cause du texte des
       boutons. L'estimateur coupait donc la bande en morceaux et rendait 9 px
       pour une barre de 28. Il rendait un nombre parfaitement stable, et faux.

       L'etendue est licite ICI parce que la couleur cible (255,0,0) n'existe
       NULLE PART ailleurs dans l'image : `zero.png` rend 0 ligne, et c'est ce
       qui autorise a prendre le premier et le dernier.
    """
    im = Image.open(chemin).convert("RGB")
    W, H = im.size
    px = im.load()
    taux = []
    for y in range(H):
        n = 0
        for x in range(W):
            if px[x, y] == CIBLE:
                n += 1
        taux.append(n / float(W))

    lignes = [y for y, t in enumerate(taux) if t > seuil_presence]
    if not lignes:
        return {"fichier": chemin, "W": W, "H": H, "etendue": 0, "debut": -1, "fin": -1,
                "lignes": 0, "touche_haut": False, "touche_bas": False,
                "taux_max": max(taux) if taux else 0.0}
    a, b = lignes[0], lignes[-1]
    return {
        "fichier": chemin, "W": W, "H": H,
        "etendue": b - a + 1, "debut": a, "fin": b, "lignes": len(lignes),
        "touche_haut": a <= 1, "touche_bas": b >= H - 2,
        "taux_max": max(taux),
    }


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: mesure_bande.py <capture.png> [...]")
        sys.exit(2)
    for chemin in sys.argv[1:]:
        try:
            r = mesure(chemin)
        except Exception as e:  # noqa: BLE001 -- on RAPPORTE l'echec, on ne le tait pas
            print("%-28s ECHEC DE LECTURE : %s" % (chemin, e))
            continue
        ancrage = "haut" if r["touche_haut"] else ("bas" if r["touche_bas"] else "flottante")
        print(
            "%-34s %dx%d  etendue=%3d px  y%d..y%d  ancrage=%-9s lignes=%3d  taux_max=%.2f"
            % (r["fichier"], r["W"], r["H"], r["etendue"], r["debut"], r["fin"], ancrage,
               r["lignes"], r["taux_max"])
        )
