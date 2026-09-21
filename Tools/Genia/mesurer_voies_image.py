# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/mesurer_voies_image.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# LE MEME JUGE POUR LES VOIES (b) ET (c) : un maillage TripoSR, d'ou qu'il vienne.
#
# TripoSR rend un objet NORMALISE (taille ~1, sans unite) : les fourchettes en
# metres du jeu d'epreuve ne s'appliquent donc pas telles quelles. On compare
# des RAPPORTS, derives des memes fourchettes ecrites d'avance :
#   H/L dans [Hmin/Lmax, Hmax/Lmin]   et   l/L dans [lmin/Lmax, 1]
# plus ce qu'un maillage unique ne peut pas donner : les parties nommees (C1 est
# toujours 1 partie), et les MORCEAUX (composantes connexes : des debris
# flottants = « rien ne flotte » rouge).
# ⚠️ Un rapport juste ne dit pas que l'objet ressemble : ouvrir les images.
#
# USAGE : python mesurer_voies_image.py --jeu <jeu.txt> --dossier <d> --suffixe _triposr.glb
# -----------------------------------------------------------------------------
import argparse
import os
import sys

import numpy as np

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from epreuve_creation import lire_jeu  # le MEME lecteur de jeu que le juge de l'assemblage


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jeu", required=True)
    ap.add_argument("--dossier", required=True)
    ap.add_argument("--suffixe", default="_triposr.glb")
    a = ap.parse_args()
    import trimesh
    ok_prop = ok_morc = n = 0
    print("%-10s | %-6s %-16s | %-6s %-11s | %-8s | %s" % ("cle", "H/L", "fourchette", "l/L", "fourchette", "morceaux", "sommets"))
    for it in lire_jeu(a.jeu):
        p = os.path.join(a.dossier, it["cle"] + a.suffixe)
        if not os.path.isfile(p):
            print("%-10s | absent (%s)" % (it["cle"], p))
            continue
        n += 1
        m = trimesh.load(p, force="mesh", process=True)
        e = m.bounds[1] - m.bounds[0]
        L, l = max(e[0], e[2]), min(e[0], e[2])
        hl, ll = e[1] / L, l / L
        fh = (it["H"][0] / it["L"][1], it["H"][1] / it["L"][0])
        fl = (it["l"][0] / it["L"][1], 1.0)
        bon = fh[0] <= hl <= fh[1] and fl[0] <= ll <= fl[1]
        morc = len(m.split(only_watertight=False))
        ok_prop += bon
        ok_morc += morc == 1
        print("%-10s | %-6.2f [%.2f,%.2f]%s | %-6.2f [%.2f,1]%s | %-8d | %d" % (
            it["cle"], hl, fh[0], fh[1], "" if fh[0] <= hl <= fh[1] else "!", ll, fl[0],
            "" if fl[0] <= ll <= fl[1] else "!", morc, len(m.vertices)))
    print("\n  proportions dans la fourchette : %d / %d ; un seul morceau : %d / %d ; parties nommees : 1 partout"
          % (ok_prop, n, ok_morc, n))
    return 0


if __name__ == "__main__":
    sys.exit(main())
