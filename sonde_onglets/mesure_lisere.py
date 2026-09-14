#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""mesure_lisere.py — la LARGEUR de l'onglet actif, lue dans l'image du produit.

AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

Canal `onglets.questions.md`, (o1) : « une mutation dans la coquille doit
changer les DEUX, ou le partage est une fiction. »

CE QUE CE SCRIPT MESURE, ET POURQUOI CETTE CIBLE-LA
  Le lisere accent de l'onglet ACTIF fait exactement la largeur de l'onglet
  (`p.Fill({tr.x, ..., tr.w, accH}, s.accent)`). C'est donc la seule bande de
  pixels de l'image dont la longueur EST la grandeur mutee. On n'a pas a faire
  confiance a un nombre que l'application imprime : on mesure ce qui est PEINT.

⚠️ LA COULEUR N'EST PAS ECRITE ICI. On cherche la plus longue suite horizontale
   de pixels FRANCHEMENT BLEUS (b nettement superieur a r et g) dans la bande
   d'onglets. NK3DModeler et Nogee partent tous deux de `NkTheme::Dark()`, dont
   l'accent est le meme bleu -- mais le script ne le suppose pas : il rapporte
   la couleur qu'il a trouvee, et c'est a la lecture de juger.

⚠️ CE QU'IL NE PEUT PAS FAIRE : comparer deux applications entre elles. Leurs
   libelles different (« Scene » contre « Scène sans titre »), donc leurs
   largeurs aussi -- la largeur vaut `TextWidth(libelle) + tab_pad_x`. Ce qui se
   compare, c'est CHAQUE application avant et apres la mutation. Le DELTA doit
   etre le meme des deux cotes.
"""
import sys
from PIL import Image


def lisere(chemin, y0=0, y1=140):
    im = Image.open(chemin).convert("RGB")
    W, H = im.size
    px = im.load()
    y1 = min(y1, H)
    best = (0, -1, -1, None)  # longueur, y, x0, couleur
    for y in range(y0, y1):
        x = 0
        while x < W:
            r, g, b = px[x, y]
            # « franchement bleu » : le canal bleu domine nettement les deux
            # autres. Aucun gris du theme sombre ne passe ce test.
            if b > 120 and b > r + 40 and b > g + 30:
                x0 = x
                while x < W:
                    r2, g2, b2 = px[x, y]
                    if not (b2 > 120 and b2 > r2 + 40 and b2 > g2 + 30):
                        break
                    x += 1
                if x - x0 > best[0]:
                    best = (x - x0, y, x0, px[x0, y])
            else:
                x += 1
    return {"fichier": chemin, "W": W, "H": H, "largeur": best[0], "y": best[1],
            "x0": best[2], "couleur": best[3]}


if __name__ == "__main__":
    for c in sys.argv[1:]:
        d = lisere(c)
        print(f"  {d['fichier']}")
        print(f"      lisere accent : largeur={d['largeur']} px  a y={d['y']} x0={d['x0']}  "
              f"couleur={d['couleur']}")
