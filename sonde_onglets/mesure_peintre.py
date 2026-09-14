#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""mesure_peintre.py — (o1) les quatre routages, juges par le PIXEL.

AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

⚠️ POURQUOI CE SCRIPT NE TESTE PAS « LA PRIMITIVE A-T-ELLE DESSINE ».
   Une primitive routee vers la MAUVAISE fonction dessine, compile, et se declare
   presente. « Il y a du rouge dans la boite » serait donc vert pour un routage
   faux. Chaque test porte sur une propriete que SEULE la bonne fonction produit :

     PolygonHex    -> la FORME : les largeurs de rangees doivent VARIER.
                      Un remplissage de rectangle donnerait des largeurs egales.
     PushBlend     -> l'ARITHMETIQUE : deux gris 100 qui se recouvrent en
                      PlusLighter donnent ~200. Un `PushBlend` inerte laisse le
                      second ECRASER le premier : 100. Le nombre tranche.
     ImagePolygone -> la NON-UNIFORMITE : une texture echantillonnee porte
                      plusieurs valeurs. Un aplat n'en porterait qu'une.

⚠️ AUCUNE COORDONNEE N'EST RECOPIEE. Chaque boite est trouvee par sa couleur de
   fond, unique dans l'image. La capture du modeleur est 1,2 fois plus grande que
   son espace logique (mesure du 14/09) : toute coordonnee recopiee serait fausse,
   et c'est deja ce qui m'a fait lire +50 la ou il y avait +41,40.
"""
import sys
from PIL import Image

FONDS = {
    "PolygonHex": (0, 0, 255),
    "PushBlend": (0, 255, 0),
    "ImagePolygone": (255, 0, 255),
}


def boite(px, W, H, coul):
    """Rectangle englobant des pixels EXACTEMENT de cette couleur."""
    x0, y0, x1, y1, n = W, H, -1, -1, 0
    for y in range(H):
        for x in range(W):
            if px[x, y][:3] == coul:
                n += 1
                if x < x0: x0 = x
                if y < y0: y0 = y
                if x > x1: x1 = x
                if y > y1: y1 = y
    return (x0, y0, x1, y1, n) if n else None


def main():
    im = Image.open(sys.argv[1]).convert("RGB")
    W, H = im.size
    px = im.load()
    print(f"image {sys.argv[1]}  {W}x{H}\n")
    verdicts = []

    # ── A. PolygonHex : la FORME ────────────────────────────────────────────
    b = boite(px, W, H, FONDS["PolygonHex"])
    if not b:
        print("  PolygonHex    : boite BLEUE introuvable -> la sonde n'a pas tourne")
        verdicts.append(False)
    else:
        x0, y0, x1, y1, _ = b
        largeurs = []
        for y in range(y0, y1 + 1):
            n = sum(1 for x in range(x0, x1 + 1) if px[x, y][:3] == (255, 0, 0))
            if n:
                largeurs.append(n)
        if not largeurs:
            print("  PolygonHex    : AUCUN rouge -> la primitive n'a rien dessine")
            verdicts.append(False)
        else:
            croit = largeurs[-1] > largeurs[0] * 2
            varie = max(largeurs) > min(largeurs) * 2
            print(f"  PolygonHex    : {len(largeurs)} rangees rouges, largeur "
                  f"{largeurs[0]} -> {largeurs[len(largeurs)//2]} -> {largeurs[-1]}")
            print(f"                  forme TRIANGULAIRE (largeur qui croit) : "
                  f"{'OUI' if (croit and varie) else 'NON -- on dirait un PAVE'}")
            verdicts.append(croit and varie)

    # ── B. PushBlend : l'ARITHMETIQUE ───────────────────────────────────────
    b = boite(px, W, H, FONDS["PushBlend"])
    if not b:
        print("  PushBlend     : boite VERTE introuvable")
        verdicts.append(False)
    else:
        x0, y0, x1, y1, _ = b
        gris = {}
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                r, g, bl = px[x, y][:3]
                if r == g == bl and r > 40:  # les gris poses par la sonde
                    gris[r] = gris.get(r, 0) + 1
        tons = sorted(gris)
        print(f"  PushBlend     : tons de gris presents {tons}")
        # le recouvrement doit exister ET valoir nettement plus que le simple
        additif = any(t > 150 for t in tons) and any(90 <= t <= 130 for t in tons)
        print(f"                  recouvrement ADDITIF (~200 a cote de ~100) : "
              f"{'OUI' if additif else 'NON -- le melange est ignore'}")
        verdicts.append(additif)

    # ── C. ImagePolygone : la NON-UNIFORMITE ────────────────────────────────
    b = boite(px, W, H, FONDS["ImagePolygone"])
    if not b:
        print("  ImagePolygone : boite MAGENTA introuvable")
        verdicts.append(False)
    else:
        x0, y0, x1, y1, _ = b
        vals = {}
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                c = px[x, y][:3]
                if c != FONDS["ImagePolygone"]:
                    vals[c] = vals.get(c, 0) + 1
        print(f"  ImagePolygone : {sum(vals.values())} pixels non-fond, "
              f"{len(vals)} couleurs DISTINCTES")
        texture = len(vals) >= 3
        print(f"                  region NON UNIFORME (texture echantillonnee) : "
              f"{'OUI' if texture else 'NON -- un aplat, pas une image'}")
        verdicts.append(texture)

    print()
    ok = sum(1 for v in verdicts if v)
    print(f"  VERDICT : {ok}/{len(verdicts)} primitives PROUVEES PAR LE PIXEL")
    return 0 if ok == len(verdicts) else 1


if __name__ == "__main__":
    sys.exit(main())
