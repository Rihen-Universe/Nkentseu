# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/comparer_assemblage_triposr.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# VOIE (b) : texte -> ASSEMBLAGE de parties -> rendu par NK3DModeler -> TripoSR.
# La question de Rodolf (21/09) : quand on ne recoit que du texte, la chaine
# doit CREER les vues manquantes -- toutes. L'assemblage donne la silhouette et
# les proportions ; TripoSR est cense monter en detail par-dessus.
#
# CE QUE CE SCRIPT MESURE, ET CE QU'IL NE PEUT PAS MESURER
#   Il compare la reconstruction TripoSR a l'ASSEMBLAGE dont elle est tiree :
#   - la FIDELITE : IoU d'occupation de surface (instrument de J0, grille 48,
#     meilleur azimut au pas de 5 deg), avec son PLANCHER (l'assemblage contre
#     un AUTRE objet du jeu) et sa BORNE HAUTE (l'assemblage contre lui-meme,
#     reechantillonne). Sans plancher, un IoU ne veut rien dire.
#   - les PROPORTIONS : hauteur / plus grande etendue horizontale, des deux.
#   - ce que l'on perd ou gagne pour EDITER : parties nommees, composantes
#     connexes, sommets.
#   ⚠️ « MIEUX » N'EST PAS MESURABLE ICI. L'assemblage est la seule verite
#      geometrique disponible ; une reconstruction qui s'en ecarte peut ajouter
#      un detail juste ou inventer un defaut, et aucun chiffre ne tranche. Ce
#      script dit si TripoSR est FIDELE, et ce qu'il coute ; l'IMAGE dit le reste.
#
# USAGE (Python du venv genia-tools) :
#   python comparer_assemblage_triposr.py --dossier <course> --cles chaise,table,...
#   (le dossier contient <cle>.obj -- l'assemblage -- et <cle>_triposr.glb)
# -----------------------------------------------------------------------------
import argparse
import os
import sys

import numpy as np

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
from j0_iou_monovue import echantillonner, occuper, iou, iou_meilleur  # l'instrument EPROUVE de J0


def charger_obj(chemin):
    V, F, groupes = [], [], set()
    for l in open(chemin, encoding="utf-8", errors="replace"):
        m = l.split()
        if not m:
            continue
        if m[0] == "v":
            V.append([float(x) for x in m[1:4]])
        elif m[0] == "f":
            F.append([int(x.split("/")[0]) - 1 for x in m[1:4]])
        elif m[0] == "g":
            groupes.add(" ".join(m[1:]))
    return np.array(V, dtype=np.float64), np.array(F, dtype=np.int64), len(groupes)


def charger_glb(chemin):
    import trimesh
    sc = trimesh.load(chemin, force="scene")
    m = sc.to_geometry() if hasattr(sc, "to_geometry") else sc.dump(concatenate=True)
    comp = len(m.split(only_watertight=False)) if len(m.faces) < 400000 else -1
    return np.asarray(m.vertices, dtype=np.float64), np.asarray(m.faces, dtype=np.int64), comp


def proportions(V):
    lo, hi = V.min(axis=0), V.max(axis=0)
    e = hi - lo
    L = max(e[0], e[2])
    return e[1] / max(L, 1e-9), min(e[0], e[2]) / max(L, 1e-9)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dossier", required=True)
    ap.add_argument("--cles", required=True)
    a = ap.parse_args()
    cles = [c.strip() for c in a.cles.split(",") if c.strip()]
    ass = {}
    for c in cles:
        p = os.path.join(a.dossier, c + ".obj")
        if os.path.isfile(p):
            V, F, g = charger_obj(p)
            ass[c] = (V, F, g)
    print("VOIE (b) : assemblage -> vue 3/4 rendue par NK3DModeler -> TripoSR")
    print("%-11s | %-6s %-6s %-6s | %-13s | %-13s | %-9s %-9s | %s" % (
        "cle", "IoU", "planch", "haute", "H/L ass->tsr", "l/L ass->tsr", "parties", "morceaux", "sommets ass->tsr"))
    cles_ass = [c for c in cles if c in ass]
    for i, c in enumerate(cles_ass):
        pt = os.path.join(a.dossier, c + "_triposr.glb")
        if not os.path.isfile(pt):
            print("%-11s | pas de reconstruction (%s absent)" % (c, pt))
            continue
        Va, Fa, ga = ass[c]
        Vt, Ft, comp = charger_glb(pt)
        Pa = echantillonner(Va, Fa, 1)
        Pa2 = echantillonner(Va, Fa, 2)
        Pt = echantillonner(Vt, Ft, 3)
        occ_a = occuper(Pa)
        mesure, ang = iou_meilleur(Pt, occ_a)
        haute, _ = iou_meilleur(Pa2, occ_a)
        # PLANCHER : l'assemblage d'un AUTRE objet du jeu, au meilleur azimut.
        autre = cles_ass[(i + 1) % len(cles_ass)]
        Vo, Fo, _ = ass[autre]
        plancher, _ = iou_meilleur(echantillonner(Vo, Fo, 4), occ_a)
        ra, rt = proportions(Va), proportions(Vt)
        print("%-11s | %.3f  %.3f  %.3f  | %.2f -> %.2f   | %.2f -> %.2f   | %-9s %-9s | %d -> %d  (azimut %.0f, plancher=%s)" % (
            c, mesure, plancher, haute, ra[0], rt[0], ra[1], rt[1], "%d->1" % ga, "1->%s" % comp,
            len(Va), len(Vt), ang, autre))
    print()
    print("  IoU : TripoSR contre l'assemblage dont il est tire ; planch : l'assemblage contre un")
    print("  AUTRE objet ; haute : l'assemblage contre lui-meme reechantillonne.")
    print("  parties : groupes nommes (l'assemblage) contre un maillage unique (TripoSR).")
    print("  morceaux : composantes connexes de la reconstruction.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
