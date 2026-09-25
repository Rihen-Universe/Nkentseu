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

# ── LE SEUIL DE PLANEITE, MESURE ET NON DEVINE (25/09) ───────────────────────
# La condition etait ecrite mais pas chiffree : « une vue 3/4 determine un
# volume, pas un objet presque plan ». Douze ajustements (deux familles x six
# references) la chiffrent, et la separation ne laisse aucun doute :
#     famille table : planeite 0,323 a 0,628 -> 5 gains, 1 perte
#     famille porte : planeite 0,016 a 0,021 -> 2 gains, 4 PERTES
# ⚠️ LES DONNEES NE DONNENT PAS UN SEUIL, ELLES DONNENT UN INTERVALLE VIDE :
#    rien entre 0,0212 et 0,3228. N'importe quelle valeur de cet intervalle
#    separe les deux populations, et pretendre a mieux serait inventer une
#    precision. On prend le milieu geometrique du vide, et on l'ecrit.
# ⚠️ ET LA PLANEITE N'EXPLIQUE PAS TOUT : la famille table a PERDU sur la
#    reference « maison », dont l'objet ajuste est le MOINS plan (0,571). La
#    regle ecarte les cas ou une vue ne suffit pas ; elle ne promet pas que
#    l'ajustement soit bon partout ailleurs.
SEUIL_PLANEITE = 0.08

TMP = "logs_genia3d/crea/q17"

# Les parametres CONTINUS de chaque famille, avec leurs bornes. Les bornes sont
# celles que le constructeur applique deja (`Borne(...)` dans NkMeshFamilles.cpp)
# -- les recopier plus larges ferait chercher dans un domaine que le code ramene
# silencieusement, et l'optimiseur croirait avoir trouve.
# ⚠️ CE SONT LES BORNES DE VRAISEMBLANCE, PAS CELLES DE CONSTRUCTION, ET LA
#    DIFFERENCE VAUT UN RESULTAT. Avec les bornes de construction (porte :
#    largeur 0,6 a 8 m, hauteur 1,6 a 10 m), l'ajustement de la porte rendait
#    5,225 x 8,950 m et PERDAIT sur le controle (-0,6256). Avec les bornes de
#    vraisemblance, il rend 1,150 x 2,150 m et GAGNE :
#        1 vue  : controle 0,3627 contre 0,4609 brute  ->  +0,0982
#        2 vues : controle 0,3298                      ->  +0,1312
#    Le defaut n'etait donc pas seulement la planeite : c'etait une recherche
#    NON BORNEE, libre d'inventer une porte de 8,95 m pour gagner quelques
#    pixels de silhouette.
# ⚠️ ET CETTE TABLE EST UNE RECOPIE DE `NkFamilleDimensionPlausible`, ce qui est
#    exactement la faute que ce depot paie le plus : une liste recopiee se
#    perime. CONDITION DE RETRAIT : elle disparait le jour ou
#    `NKEditMeshHarness` imprime les intervalles, et ou ce fichier les LIT au
#    lieu de les connaitre. Je l'ecris ici plutot que de la laisser passer
#    inapercue.
DOMAINES = {
    "table": dict(largeur=(0.60, 2.60), hauteur=(0.72, 0.78), profondeur=(0.40, 1.20)),
    "porte": dict(largeur=(0.70, 1.60), hauteur=(2.00, 2.20)),
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


# LES VUES D'AJUSTEMENT. Une seule par defaut -- l'image que l'utilisateur joint.
# DEUX quand une PLANCHE est fournie (Q13) : c'est exactement la seconde vue qui
# manque a un objet presque plan, et elle est gratuite, elle est deja dans
# l'image. ⚠️ Aucune n'est une vue du JUGE (0, 90, 180) : le controle reste hors
# de l'ajustement, sinon le gain annonce serait un score d'entrainement.
# ⚠️ 45 ET 135 NE MARCHAIENT PAS, ET LA RAISON COMPTE : pour une plaque
# symetrique, ces deux angles donnent des silhouettes MIROIR -- la seconde
# n'apporte aucune information que la premiere n'avait pas, et l'ajustement a
# rendu EXACTEMENT les memes parametres (cout 0,0518 a la quatrieme decimale).
# Une seconde vue n'aide que si elle voit l'objet AUTREMENT.
AZ_AJUST2 = (45.0, 157.5)


def masque_ajustement(ident, corpus, nvues=1):
    """Les vues d'ajustement de la reference, et elles seules."""
    z = np.load(os.path.join(corpus, ident + ".npz"))
    az = AZ_AJUST2[:nvues] if nvues > 1 else (AZ_AJUST,)
    out = []
    for a in az:
        idx = int(a / 22.5) * 3 + 1          # elevation +15
        p = z["poses"][idx]
        if abs(float(p[0]) - a) > 1e-6:
            raise ValueError("pose inattendue : %s" % p)
        out.append(z["rgba"][idx, ..., 3] > 127)
    return out


def grille(dom, n):
    axes = [np.linspace(a, b, n) for (a, b) in dom.values()]
    return [dict(zip(dom.keys(), c)) for c in itertools.product(*axes)]


def ajuster(famille, ident, corpus, n, tag, nvues=1):
    """Rend (params, cout) qui minimisent C_fid sur les vues d'ajustement."""
    mref = masque_ajustement(ident, corpus, nvues)
    az = list(AZ_AJUST2[:nvues]) if nvues > 1 else [AZ_AJUST]
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
            c = cfid(mref, masques(ch, az))
            if c < best[1]:
                best = (dict(prm), c)
        if best[0] is None:
            break
        dom = {k: (max(dom[k][0], v - (dom[k][1] - dom[k][0]) / (2.0 * (n - 1))),
                   min(dom[k][1], v + (dom[k][1] - dom[k][0]) / (2.0 * (n - 1))))
               for k, v in best[0].items()}
    return best


def planeite(chemin):
    """min / max des trois etendues. 1 = cubique, 0 = plan."""
    Vt, F = R.charger(chemin)
    e = Vt.max(axis=0) - Vt.min(axis=0)
    return float(e.min() / max(e.max(), 1e-9))


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
    p.add_argument("--vues", type=int, default=1, help="1 = une image jointe ; 2 = une PLANCHE (Q13)")
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
        prm, cout = ajuster(fam, ident, a.corpus, a.pas_grille, fam, a.vues)
        ajuste = juger(fam, prm, ident, a.corpus, fam + "_ajuste") if prm else float("inf")
        pl = planeite(os.path.join(TMP, "juge_%s_ajuste.obj" % fam)) if prm else 0.0
        print("")
        print("=== %s (reference %s) ===" % (fam, ident))
        print("  planeite de l'objet ajuste : %.4f  (seuil %.2f)" % (pl, SEUIL_PLANEITE))
        if pl < SEUIL_PLANEITE:
            # ── C'ETAIT UN REFUS ; MA PROPRE MESURE L'A CONTREDIT ─────────────
            # Le seuil a ete mesure sur douze ajustements a domaine de
            # CONSTRUCTION (porte : jusqu'a 8 m de large, 10 m de haut), ou les
            # objets plans perdaient 4 fois sur 6. Avec le domaine de
            # VRAISEMBLANCE, la meme porte GAGNE (+0,1312 sur le controle).
            # Le refus ne tient donc plus, et je ne le garde pas : il devient un
            # avertissement, et le seuil est a REMESURER sur le nouveau domaine.
            print("  AVERTISSEMENT : objet presque PLAN. Une vue 3/4 contraint mal")
            print("          ses parametres -- plusieurs couples y donnent la meme")
            print("          silhouette. Mesure au domaine de CONSTRUCTION : 4 pertes")
            print("          sur 6 sous ce seuil. Mesure au domaine de VRAISEMBLANCE :")
            print("          la porte gagne. Le seuil est donc A REMESURER, et une")
            print("          seconde vue reste preferable (planche technique, Q13 :")
            print("          +0,0982 a une vue, +0,1312 a deux).")
        print("  parametres trouves : %s" % (", ".join("%s=%.3f" % (k, v) for k, v in prm.items()) if prm else "(aucun)"))
        print("  C_fid sur %d vue(s) d'AJUSTEMENT             : %.4f" % (a.vues, cout))
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
