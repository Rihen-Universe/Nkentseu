# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/ajuster_famille.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# LA FAMILLE REGARDE ENFIN L'IMAGE (Q17 — modelisation par projection).
#
# Le constat qui commande ce fichier est de moi, et il est mesure : « la famille
# produit un objet juste, pas l'objet de la reference, parce qu'elle ne regarde
# jamais l'image ». Une table de famille sort a 1,60 x 0,75 x 0,90 m quelle que
# soit la table qu'on lui montre. Ici, les PARAMETRES du formulaire sont
# cherches pour que la silhouette de la famille recouvre celle de la reference.
#
# ⚠️ ON OPTIMISE LA FAMILLE REELLE, PAS UNE COPIE. La geometrie vient de
#    `NKEditMeshHarness --famille-obj`, c'est-a-dire du meme `NkFamilleConstruire`
#    qui posera l'objet dans la scene. Une reimplementation approchee en Python
#    rendrait des parametres justes pour elle seule.
#
# ⚠️ ET SURTOUT : L'AJUSTEMENT NE VOIT QU'UNE VUE, LE JUGE EN VOIT TROIS AUTRES.
#    Un juge qui devient fonction d'objectif cesse d'etre un juge. L'ajustement
#    ne recoit que la vue 3/4 (azimut 45) -- exactement l'image que l'utilisateur
#    joint -- et le verdict est rendu sur les azimuts 0, 90 et 180, que
#    l'ajustement n'a jamais vus. Le gain annonce est donc un gain sur des vues
#    de controle, pas un score d'entrainement.
#
# ⚠️ ET LE CONTROLE NEGATIF RESTE : on ajuste AUSSI la famille sur les autres
#    references. Si la famille ajustee sur la reference A colle aussi bien a B,
#    l'ajustement n'a rien appris de A.
#
# USAGE : python ajuster_famille.py --cle table --famille table
#                                   [--pas-grille 5] [--sortie DOSSIER]
# -----------------------------------------------------------------------------
import argparse
import itertools
import os
import subprocess
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

HARNESS = os.path.abspath("Build/Bin/Release-Windows/NKEditMeshHarness/NKEditMeshHarness.exe")
AZ_AJUST = 45.0          # la vue que l'utilisateur joint : la SEULE que l'ajustement voit
TMP = "logs_genia3d/crea/q17"

# Les parametres CONTINUS de chaque famille, avec leurs bornes. Les bornes sont
# celles que le constructeur applique deja (`Borne(...)` dans NkMeshFamilles.cpp)
# -- les recopier plus larges ferait chercher dans un domaine que le code ramene
# silencieusement, et l'optimiseur croirait avoir trouve.
DOMAINES = {
    "table": dict(largeur=(0.4, 4.0), hauteur=(0.3, 1.2), profondeur=(0.4, 2.0)),
    "porte": dict(largeur=(0.6, 8.0), hauteur=(1.6, 10.0)),
}


def construire(famille, params, chemin):
    cmd = [HARNESS, "--famille-obj", chemin, "--nom", famille]
    for k, v in params.items():
        cmd += ["--" + k, "%.4f" % v]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode == 0 and os.path.exists(chemin)


def masques(chemin, azimuts, taille=384):
    Vt, F = R.charger(chemin)
    out = []
    for az in azimuts:
        p = R.pose_depuis_angles(az, FR.ELEVATION, FR.RAYON, (0, 0, 0), FR.FOV,
                                 taille, taille, 0.05, 10.0)
        _, m, _ = R.rendre_vectorise(Vt, F, p)
        out.append(m)
    return out


def cfid(mref, mcand):
    """La MEME distance que le juge : d_prop + (1 - iou_brut), moyennee."""
    v, e, br = FR.comparer(mref, mcand)
    return e + (1.0 - br)


def masque_ajustement(ident, corpus):
    """La vue 3/4 de la reference, et elle seule."""
    z = np.load(os.path.join(corpus, ident + ".npz"))
    idx = 2 * 3 + 1               # azimut 45, elevation +15
    p = z["poses"][idx]
    if abs(float(p[0]) - AZ_AJUST) > 1e-6:
        raise ValueError("pose inattendue : %s" % p)
    return [z["rgba"][idx, ..., 3] > 127]


def grille(dom, n):
    axes = [np.linspace(a, b, n) for (a, b) in dom.values()]
    return [dict(zip(dom.keys(), c)) for c in itertools.product(*axes)]


def ajuster(famille, ident, corpus, n, tag):
    """Rend (params, cout) qui minimisent C_fid sur la SEULE vue d'ajustement."""
    mref = masque_ajustement(ident, corpus)
    dom = DOMAINES[famille]
    best = (None, float("inf"))
    # Deux passes : grille grossiere, puis grille fine autour du gagnant. Une
    # descente de gradient n'a pas de sens ici -- la fonction est en escalier,
    # le rasteriseur ne rend que des pixels entiers.
    for passe in range(2):
        cands = grille(dom, n)
        for prm in cands:
            ch = os.path.join(TMP, "ajust_%s.obj" % tag)
            if not construire(famille, prm, ch):
                continue
            c = cfid(mref, masques(ch, [AZ_AJUST]))
            if c < best[1]:
                best = (dict(prm), c)
        if best[0] is None:
            break
        dom = {k: (max(dom[k][0], v - (dom[k][1] - dom[k][0]) / (2.0 * (n - 1))),
                   min(dom[k][1], v + (dom[k][1] - dom[k][0]) / (2.0 * (n - 1))))
               for k, v in best[0].items()}
    return best


def juger(famille, params, ident, corpus, tag):
    """Le VERDICT, sur les trois vues que l'ajustement n'a jamais vues."""
    mref = FR.masque_reference(ident, corpus)
    ch = os.path.join(TMP, "juge_%s.obj" % tag)
    if not construire(famille, params, ch):
        return float("inf")
    return cfid(mref, masques(ch, FR.AZ_VUES))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--jeu", default="Tools/Genia/epreuve_fidelite.txt")
    p.add_argument("--corpus", default=FR.CORPUS)
    p.add_argument("--pas-grille", type=int, default=5)
    p.add_argument("--familles", default="table,porte")
    a = p.parse_args()
    os.makedirs(TMP, exist_ok=True)

    jeu = {}
    for l in open(a.jeu, encoding="utf-8"):
        l = l.strip()
        if not l or l.startswith("#"):
            continue
        c = [x.strip() for x in l.split("|")]
        if len(c) >= 4:
            jeu[c[0]] = c[1]

    for fam in a.familles.split(","):
        if fam not in jeu or fam not in DOMAINES:
            print("famille inconnue ou absente du jeu : %s" % fam)
            continue
        ident = jeu[fam]
        # 1. LA FAMILLE NON AJUSTEE : ses defauts, c'est-a-dire ce que Rodolf voit.
        brut = juger(fam, {}, ident, a.corpus, fam + "_brut")
        # 2. L'AJUSTEMENT, sur la seule vue 3/4.
        prm, cout = ajuster(fam, ident, a.corpus, a.pas_grille, fam)
        ajuste = juger(fam, prm, ident, a.corpus, fam + "_ajuste") if prm else float("inf")
        print("")
        print("=== %s (reference %s) ===" % (fam, ident))
        print("  parametres trouves : %s" % (", ".join("%s=%.3f" % (k, v) for k, v in prm.items()) if prm else "(aucun)"))
        print("  C_fid sur la vue d'AJUSTEMENT (45 deg)      : %.4f" % cout)
        print("  C_fid sur les TROIS vues de CONTROLE        : %.4f  (famille brute : %.4f)"
              % (ajuste, brut))
        print("  gain sur le controle                        : %+.4f" % (brut - ajuste))
        # 3. LE CONTROLE NEGATIF : la famille ajustee pour CETTE reference
        #    colle-t-elle aussi bien aux AUTRES ? Si oui, elle n'a rien appris.
        for autre, idAutre in jeu.items():
            if autre == fam:
                continue
            c = juger(fam, prm, idAutre, a.corpus, fam + "_neg")
            print("    sur la reference « %-8s » (mauvaise)      : %.4f  %s"
                  % (autre, c, "" if c > ajuste else "<-- COLLE MIEUX QU'A LA SIENNE"))


if __name__ == "__main__":
    main()
