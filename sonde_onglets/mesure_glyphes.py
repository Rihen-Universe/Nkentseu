#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""mesure_glyphes.py — l'ENCRE de chaque glyphe, et la geometrie des onglets.

AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

Canal `onglets.questions.md`, mesures (o1) et (o2).

POURQUOI CE SCRIPT NE SAIT RIEN DE LA SONDE
  Il ne porte AUCUN attendu en dur. Les boites et les attendus sont lus dans le
  journal `[BOITE]` que la sonde imprime a l'instant meme de la capture ; les
  rectangles d'onglets dans `[ONGLET]`. Un attendu recopie ici se perimerait le
  jour ou la sonde changerait une position, et crierait rouge sur une image
  correcte -- ce depot a deja paye « un attendu en dur se perime ».

POURQUOI L'ENCRE, ET PAS UNE COMPARAISON D'IMAGES
  La sonde peint chaque glyphe en BLANC PUR (255,255,255) sur un fond NOIR PUR
  (0,0,0) qu'elle pose elle-meme. Le theme sombre du kit part de #212121 : le
  noir pur n'existe nulle part ailleurs dans l'image. « Y a-t-il de l'encre dans
  cette boite » est donc une question sans ambiguite, et elle ne depend ni de
  l'anticrenelage ni du theme.

⚠️ LE VERDICT EST A DEUX FACES, ET C'EST LE POINT
  Un banc qui ne sait dire que « oui » ne mesure rien. On exige donc les DEUX :
  les glyphes DANS la plage doivent porter de l'encre, ceux HORS plage doivent
  en porter VISIBLEMENT MOINS (le glyphe de repli, s'il y en a un). Si les deux
  familles rendent la meme chose, la mesure ne conclut pas -- et le script le
  dit au lieu de trancher.
"""

import re
import sys
from PIL import Image


def lit_journal(chemin):
    boites, onglets = [], []
    for l in open(chemin, encoding="utf-8", errors="replace"):
        m = re.match(r"\[BOITE\] (\d+)\s+U\+([0-9A-F]+)\s+attendu=(\d)\s+"
                     r"x=([-\d.]+) y=([-\d.]+) w=([-\d.]+) h=([-\d.]+)", l)
        if m:
            boites.append(dict(i=int(m.group(1)), cp=int(m.group(2), 16),
                               attendu=int(m.group(3)), x=float(m.group(4)),
                               y=float(m.group(5)), w=float(m.group(6)),
                               h=float(m.group(7))))
        m = re.match(r"\[ONGLET\] (\d+)\s+id=(\d+)\s+x=([-\d.]+) y=([-\d.]+) "
                     r"w=([-\d.]+) h=([-\d.]+)", l)
        if m:
            onglets.append(dict(i=int(m.group(1)), id=int(m.group(2)),
                                x=float(m.group(3)), y=float(m.group(4)),
                                w=float(m.group(5)), h=float(m.group(6))))
    return boites, onglets


def encre(px, W, H, b, seuil=40):
    """Nombre de pixels CLAIRS dans la boite, et leur etendue verticale.

    L'etendue compte autant que le nombre : un glyphe de repli (le carre) a
    beaucoup d'encre sur son CONTOUR ; un accent, lui, monte plus haut que la
    lettre. On rend les deux plutot qu'un seul nombre qui melangerait les cas.
    """
    x0, y0 = int(b["x"]), int(b["y"])
    x1, y1 = int(b["x"] + b["w"]), int(b["y"] + b["h"])
    x0, y0 = max(0, x0), max(0, y0)
    x1, y1 = min(W, x1), min(H, y1)
    n, lignes = 0, []
    for y in range(y0, y1):
        ligne = 0
        for x in range(x0, x1):
            r, g, bl = px[x, y][:3]
            if r > seuil and g > seuil and bl > seuil:
                ligne += 1
        if ligne:
            lignes.append(y)
        n += ligne
    haut = (lignes[0] - y0) if lignes else -1
    bas = (lignes[-1] - y0) if lignes else -1
    return n, haut, bas


def main():
    img, jour = sys.argv[1], sys.argv[2]
    im = Image.open(img).convert("RGB")
    W, H = im.size
    px = im.load()
    boites, onglets = lit_journal(jour)

    print(f"image {img}  {W}x{H}   boites={len(boites)}  onglets={len(onglets)}")
    print()
    print("(o2) L'ENCRE PAR GLYPHE — attendu lu dans le journal, pas ecrit ici")
    print(f"  {'cp':>8} {'attendu':>8} {'encre':>7} {'haut':>5} {'bas':>5}  verdict")
    dedans, dehors = [], []
    for b in boites:
        n, haut, bas = encre(px, W, H, b)
        (dedans if b["attendu"] else dehors).append(n)
        v = "encre" if n > 0 else "VIDE"
        print(f"  U+{b['cp']:04X} {b['attendu']:>8} {n:>7} {haut:>5} {bas:>5}  {v}")

    print()
    ok_dedans = all(n > 0 for n in dedans)
    print(f"  DANS la plage : {len(dedans)} glyphes, encre min={min(dedans) if dedans else 0}, "
          f"max={max(dedans) if dedans else 0}  -> {'TOUS ENCRES' if ok_dedans else 'UN AU MOINS VIDE'}")
    print(f"  HORS la plage : {len(dehors)} glyphes, encre {dehors}")

    # ⚠️ LE DISCRIMINANT. Si les deux familles se ressemblent, la mesure NE
    #    CONCLUT PAS, et on le dit -- on ne repeint pas un « vert » par defaut.
    if dedans and dehors:
        if all(n == dehors[0] for n in dehors) and dehors[0] not in dedans:
            print("  -> les deux familles DIFFERENT : le banc sait dire non. Mesure VALIDE.")
        elif set(dehors) & set(dedans):
            print("  -> ⚠️ les deux familles se CONFONDENT : ce banc ne discrimine pas, "
                  "la mesure NE CONCLUT PAS.")

    print()
    print("(o1) LA GEOMETRIE DES ONGLETS — rapportee par le composant PARTAGE")
    for o in onglets:
        print(f"  onglet {o['i']} id={o['id']}  x={o['x']:.2f} w={o['w']:.2f} h={o['h']:.2f}")
    if onglets:
        print(f"  hauteur commune : {set(round(o['h'], 2) for o in onglets)}")


if __name__ == "__main__":
    main()
