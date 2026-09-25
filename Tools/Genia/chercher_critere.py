# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/chercher_critere.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# CHERCHER LE CRITERE QUI SEPARE, AU LIEU DE LE DEVINER (25/09).
#
# Deux criteres ont deja ete REFUTES par leur propre controle negatif :
#   - C_forme (IoU apres renormalisation 2D) : les negatifs montaient a 0,71-0,84,
#     parce que la renormalisation rend deux objets quelconques semblables ;
#   - C_brut (IoU sans renormalisation) : pire encore, 1 premier sur 6 -- deux
#     objets ramenes a la meme boite 3D se recouvrent largement par leur masse.
#
# Ce script ne propose donc RIEN a priori. Il calcule, pour chaque couple
# (reference, candidat), un ensemble de grandeurs independantes, puis il MESURE
# laquelle separe : combien de fois le bon candidat arrive premier, et de
# combien il bat le meilleur des mauvais. C'est le controle negatif qui elit le
# critere, pas moi.
#
# ⚠️ L'ANGLE EST CHOISI UNE FOIS, PAR UNE GRANDEUR QUI N'EST PAS JUGEE ENSUITE
#    (l'IoU apres renormalisation, c'est-a-dire la FORME seule). Choisir l'angle
#    avec le critere qu'on evalue lui donnerait le meilleur des deux mondes.
#
# ⚠️ ET SIX OBJETS, C'EST SIX OBJETS. Un critere qui gagne ici a ete choisi sur
#    30 couples negatifs ; il faudra le reprouver sur un jeu qu'il n'a pas vu.
#
# USAGE : python chercher_critere.py --candidats DOSSIER [--sortie f.csv]
# -----------------------------------------------------------------------------
import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fidelite_reference as FR
import rendre_vues_corpus as R

for _f in (sys.stdout, sys.stderr):
    try:
        _f.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def contour(m):
    """Le bord du masque : un pixel allume dont un voisin est eteint."""
    p = np.zeros_like(m)
    p[1:-1, 1:-1] = (m[1:-1, 1:-1] & ~(m[:-2, 1:-1] & m[2:, 1:-1] &
                                       m[1:-1, :-2] & m[1:-1, 2:]))
    return p


def grandeurs(mref, mcand):
    """Toutes les grandeurs d'un couple, moyennees sur les trois vues."""
    g = {k: [] for k in ("iou_recadre", "iou_brut", "d_prop", "d_remplissage",
                         "d_contour", "d_profil_h", "d_profil_v")}
    for r, c in zip(mref, mcand):
        rr, ar = FR.recadrer(r)
        cc, ac = FR.recadrer(c)
        g["iou_recadre"].append(FR.iou(rr, cc))
        g["iou_brut"].append(FR.iou(r, c))
        g["d_prop"].append(abs(np.log(max(ac, 1e-6)) - np.log(max(ar, 1e-6))))
        # REMPLISSAGE : la part de la boite que la silhouette occupe. Une chaise
        # (des vides entre les pieds) et une porte (pleine) s'y separent, la ou
        # l'IoU les confond.
        fr_, fc_ = rr.mean(), cc.mean()
        g["d_remplissage"].append(abs(np.log(max(fc_, 1e-6)) - np.log(max(fr_, 1e-6))))
        # CONTOUR : la distance moyenne d'un point du bord de l'un au bord de
        # l'autre, en pixels de la boite normalisee. L'IoU compte de la MASSE ;
        # le contour compte de la FORME, et deux objets massifs qui se
        # recouvrent peuvent avoir des bords tres differents.
        br, bc = contour(rr), contour(cc)
        ys, xs = np.nonzero(br)
        yt, xt = np.nonzero(bc)
        if len(ys) and len(yt):
            P = np.stack([ys, xs], 1).astype(np.float64)
            Q = np.stack([yt, xt], 1).astype(np.float64)
            pas = max(1, len(P) // 400)
            pat = max(1, len(Q) // 400)
            P, Q = P[::pas], Q[::pat]
            d1 = np.sqrt(((P[:, None, :] - Q[None, :, :]) ** 2).sum(-1)).min(1).mean()
            d2 = np.sqrt(((Q[:, None, :] - P[None, :, :]) ** 2).sum(-1)).min(1).mean()
            g["d_contour"].append(0.5 * (d1 + d2) / rr.shape[0])
        else:
            g["d_contour"].append(1.0)
        # PROFILS : la part allumee par colonne et par ligne. C'est la silhouette
        # reduite a deux courbes ; elle distingue « large en bas » de « large en
        # haut » la ou l'aire seule ne le fait pas.
        g["d_profil_v"].append(float(np.abs(rr.mean(1) - cc.mean(1)).mean()))
        g["d_profil_h"].append(float(np.abs(rr.mean(0) - cc.mean(0)).mean()))
    return {k: float(np.mean(v)) for k, v in g.items()}


# Sens de chaque grandeur : +1 = grand vaut mieux, -1 = petit vaut mieux.
SENS = dict(iou_recadre=+1, iou_brut=+1, d_prop=-1, d_remplissage=-1,
            d_contour=-1, d_profil_h=-1, d_profil_v=-1)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--jeu", default="Tools/Genia/epreuve_fidelite.txt")
    p.add_argument("--candidats", required=True)
    p.add_argument("--corpus", default=FR.CORPUS)
    p.add_argument("--pas", type=float, default=15.0)
    p.add_argument("--taille", type=int, default=384)
    p.add_argument("--sortie")
    a = p.parse_args()

    jeu = []
    for l in open(a.jeu, encoding="utf-8"):
        l = l.strip()
        if not l or l.startswith("#"):
            continue
        c = [x.strip() for x in l.split("|")]
        if len(c) >= 4:
            jeu.append(dict(cle=c[0], ident=c[1]))
    cands = {}
    for it in jeu:
        for ext in (".obj", ".glb"):
            ch = os.path.join(a.candidats, it["cle"] + ext)
            if os.path.exists(ch):
                cands[it["cle"]] = ch
                break

    res = {}
    for it in jeu:
        mref = FR.masque_reference(it["ident"], a.corpus)
        for cle, ch in cands.items():
            best, bg = -1.0, None
            for d in np.arange(0.0, 360.0, a.pas):
                m = FR.masques_candidat(ch, float(d), a.taille)
                g = grandeurs(mref, m)
                if g["iou_recadre"] > best:      # l'angle, par la FORME seule
                    best, bg = g["iou_recadre"], g
            res[(it["cle"], cle)] = bg
            print("  %-8s vs %-8s %s" % (it["cle"], cle,
                  " ".join("%s=%.4f" % (k, bg[k]) for k in sorted(bg))))
            sys.stdout.flush()

    n = len(cands)
    print("")
    print("%-16s %-8s %-10s %-10s %s" % ("grandeur", "rang 1", "marge med.", "marge min.", "verdict"))
    for k in sorted(SENS):
        s = SENS[k]
        prem, marges = 0, []
        for it in jeu:
            c = it["cle"]
            if (c, c) not in res:
                continue
            v = res[(c, c)][k]
            autres = [res[(c, o)][k] for o in cands if o != c]
            meilleur_faux = max(autres) if s > 0 else min(autres)
            prem += (v > meilleur_faux) if s > 0 else (v < meilleur_faux)
            marges.append(s * (v - meilleur_faux))
        med = float(np.median(marges)) if marges else 0.0
        mn = float(np.min(marges)) if marges else 0.0
        print("%-16s %d/%d      %-+10.4f %-+10.4f %s"
              % (k, prem, n, med, mn,
                 "SEPARE TOUJOURS" if prem == n else
                 ("separe %d fois" % prem if prem > 1 else "ne separe pas")))
    if a.sortie:
        with open(a.sortie, "w", encoding="utf-8") as f:
            cles = sorted(SENS)
            f.write("reference,candidat," + ",".join(cles) + "\n")
            for (r, c), g in res.items():
                f.write("%s,%s,%s\n" % (r, c, ",".join("%.6f" % g[k] for k in cles)))
        print("\ndetail : %s" % a.sortie)


if __name__ == "__main__":
    main()
