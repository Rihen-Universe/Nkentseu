#!/usr/bin/env python3
"""
comparer-captures.py -- L'IMAGE METAL EST-ELLE LA BONNE ? (2026-09-30)
=============================================================================
POURQUOI IL EXISTE

  Une capture qui s'ecrit n'est pas une capture juste : un rendu tout noir, ou
  tout d'une couleur, s'ecrit tres bien. Ce script relit les PNG (sans
  dependance : zlib de Python) et verifie quelques pixels, de deux facons :

    1. des REGLES sur la scene elle-meme (--regles) :
         tuto03 : camera visant le cube DORE au centre -> le pixel du centre
                  est « or » (rouge nettement au-dessus du bleu), et l'image
                  n'est pas unie ;
         tuto02 : ecran vide + panneau de texte -> l'image n'est pas unie ;
    2. une COMPARAISON a la capture du rendu logiciel de la meme scene
       (--reference), point par point sur une grille : rapportee en clair,
       et en echec seulement si l'image est franchement autre (plus de la
       moitie des points a plus de --tolerance).

  Le rapport (texte) est ecrit a cote des images, pour l'artefact du run.

USAGE
  comparer-captures.py --image metal.png [--reference software.png]
                       [--regles tuto03|tuto02] [--tolerance 90] [--rapport f.txt]
  Sortie 0 : conforme. Sortie 1 : non conforme (annotation ::error).
"""
import argparse
import struct
import sys
import zlib


def lire_png(chemin):
    """(largeur, hauteur, lignes) ; lignes = liste de bytes RGBA (8 bits)."""
    with open(chemin, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{chemin} : pas un PNG")
    pos, idat, w, h, prof, typ, entrelace = 8, b"", 0, 0, 0, 0, 0
    while pos < len(data):
        n = struct.unpack(">I", data[pos:pos + 4])[0]
        genre = data[pos + 4:pos + 8]
        corps = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if genre == b"IHDR":
            w, h, prof, typ, _, _, entrelace = struct.unpack(">IIBBBBB", corps)
        elif genre == b"IDAT":
            idat += corps
        elif genre == b"IEND":
            break
    if prof != 8 or entrelace:
        raise ValueError(f"{chemin} : PNG {prof} bits / entrelace={entrelace} non gere")
    canaux = {6: 4, 2: 3, 0: 1, 4: 2}.get(typ)
    if canaux is None:
        raise ValueError(f"{chemin} : type de couleur {typ} non gere")
    brut = zlib.decompress(idat)
    pas = w * canaux
    lignes, prec = [], bytearray(pas)
    i = 0
    for _ in range(h):
        filtre = brut[i]
        ligne = bytearray(brut[i + 1:i + 1 + pas])
        i += 1 + pas
        for x in range(pas):
            a = ligne[x - canaux] if x >= canaux else 0
            b = prec[x]
            c = prec[x - canaux] if x >= canaux else 0
            if filtre == 1:
                ligne[x] = (ligne[x] + a) & 0xFF
            elif filtre == 2:
                ligne[x] = (ligne[x] + b) & 0xFF
            elif filtre == 3:
                ligne[x] = (ligne[x] + ((a + b) >> 1)) & 0xFF
            elif filtre == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                ligne[x] = (ligne[x] + pr) & 0xFF
        prec = ligne
        if canaux == 4:
            lignes.append(bytes(ligne))
        else:
            rgba = bytearray()
            for x in range(w):
                px = ligne[x * canaux:(x + 1) * canaux]
                if canaux == 3:
                    rgba += px + b"\xff"
                elif canaux == 1:
                    rgba += bytes([px[0]] * 3) + b"\xff"
                else:
                    rgba += bytes([px[0]] * 3) + bytes([px[1]])
            lignes.append(bytes(rgba))
    return w, h, lignes


def pixel(img, fx, fy):
    """Moyenne 5x5 autour du point (fraction de la largeur / hauteur)."""
    w, h, lignes = img
    cx, cy = int(fx * (w - 1)), int(fy * (h - 1))
    s = [0, 0, 0]
    n = 0
    for y in range(max(0, cy - 2), min(h, cy + 3)):
        for x in range(max(0, cx - 2), min(w, cx + 3)):
            px = lignes[y][x * 4:x * 4 + 3]
            for k in range(3):
                s[k] += px[k]
            n += 1
    return tuple(v // max(n, 1) for v in s)


def ecart(a, b):
    return sum((a[k] - b[k]) ** 2 for k in range(3)) ** 0.5


def uniformite(img):
    """Ecart-type de la luminance sur une grille 16x16."""
    vals = []
    for j in range(16):
        for i in range(16):
            r, g, b = pixel(img, (i + 0.5) / 16, (j + 0.5) / 16)
            vals.append(0.299 * r + 0.587 * g + 0.114 * b)
    m = sum(vals) / len(vals)
    return (sum((v - m) ** 2 for v in vals) / len(vals)) ** 0.5


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--reference")
    ap.add_argument("--regles", default="")
    ap.add_argument("--tolerance", type=float, default=90.0)
    ap.add_argument("--rapport")
    a = ap.parse_args()

    lignes, echecs = [], []
    img = lire_png(a.image)
    lignes.append(f"image : {a.image} ({img[0]}x{img[1]})")
    sigma = uniformite(img)
    lignes.append(f"ecart-type de luminance (grille 16x16) : {sigma:.1f}")
    if sigma < 4.0:
        echecs.append(f"image quasi UNIE (ecart-type {sigma:.1f}) : rien n'a ete dessine")

    if a.regles == "tuto03":
        centre = pixel(img, 0.5, 0.5)
        lignes.append(f"centre (cube dore attendu) : RGB {centre}")
        if not (centre[0] > centre[2] + 20 and centre[0] > 40):
            echecs.append(f"le centre n'est pas dore : RGB {centre} (rouge > bleu + 20 attendu)")

    import os
    if a.reference and not os.path.isfile(a.reference):
        lignes.append(f"reference absente ({a.reference}) : comparaison sautee")
        a.reference = None
    if a.reference:
        ref = lire_png(a.reference)
        lignes.append(f"reference : {a.reference} ({ref[0]}x{ref[1]})")
        if (ref[0], ref[1]) != (img[0], img[1]):
            lignes.append("tailles differentes : comparaison sur les positions relatives")
        loin, total = 0, 0
        lignes.append("point        image            reference        ecart")
        for fy in (0.15, 0.35, 0.5, 0.65, 0.85):
            for fx in (0.15, 0.35, 0.5, 0.65, 0.85):
                p, q = pixel(img, fx, fy), pixel(ref, fx, fy)
                e = ecart(p, q)
                total += 1
                loin += e > a.tolerance
                lignes.append(f"({fx:.2f},{fy:.2f})  {str(p):16} {str(q):16} {e:6.1f}")
        lignes.append(f"points a plus de {a.tolerance:.0f} : {loin}/{total}")
        if loin * 2 > total:
            echecs.append(f"{loin}/{total} points loin du rendu logiciel (> {a.tolerance:.0f})")

    rapport = "\n".join(lignes + [("NON CONFORME : " + " ; ".join(echecs)) if echecs else "CONFORME"])
    print(rapport)
    if a.rapport:
        with open(a.rapport, "w", encoding="utf-8") as f:
            f.write(rapport + "\n")
    esc = rapport.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
    titre = a.image.split("/")[-1]
    if echecs:
        print(f"::error title={titre}::{esc}")
        return 1
    print(f"::notice title={titre} conforme::{esc}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
