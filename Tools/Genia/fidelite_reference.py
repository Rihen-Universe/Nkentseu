# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/fidelite_reference.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# « EST-CE CET OBJET-LA ? » — LA FIDELITE A LA REFERENCE, MESUREE (Q14.1).
#
# Rodolf, 22/09 : « certes le block out est correct, mais ne rend pas l'objet
# FIDELE A LA CAPTURE ». Le critere n'est plus « ca ressemble a une chaise »,
# c'est « c'est CETTE chaise ». Les criteres et les seuils sont ecrits dans
# `Tools/Genia/epreuve_fidelite.txt`, AVANT la premiere course.
#
# CE QU'IL FAIT. Il compare la SILHOUETTE de l'objet produit a celle de la
# reference, sur TROIS vues de la reference (azimuts 0, 90 et 180 degres a la
# meme elevation), le candidat etant vu sous UN SEUL decalage d'azimut commun
# aux trois. Un candidat qui tomberait juste sur une vue par chance ne peut pas
# tomber juste sur trois avec le meme decalage.
#
# ⚠️ TROIS PIEGES QU'IL EVITE, ET CHACUN A DEJA ETE PAYE ICI :
#
#  1. IL PROUVE SON ZERO AVANT DE JUGER. La reference comparee a elle-meme doit
#     rendre IoU = 1,000000 EXACTEMENT. Un comparateur qui ne rend pas 1 sur
#     l'identite ne mesure rien, et tout ce qui suit serait du bruit. En cas
#     d'echec il se declare HORS SERVICE (code 3) au lieu de rendre un verdict.
#
#  2. IL EXIGE UN CONTROLE NEGATIF. Un IoU de 0,6 ne veut rien dire tant qu'on
#     ne sait pas ce que rend un objet QUI N'EST PAS LE BON. Le mode `--negatif`
#     croise chaque reference avec les candidats des AUTRES. La fusion
#     multi-vues de 2026 est morte exactement la : IoU 0,299 semblait honorable
#     jusqu'a ce qu'on mesure que le hasard rendait 0,300.
#
#  3. IL SEPARE LA FORME DE LA TAILLE, ET MESURE LES DEUX. `C_forme` ramene les
#     deux masques a la MEME boite englobante : il ne juge que la forme.
#     `C_prop` mesure ce que cette normalisation vient de retirer -- l'ecart de
#     rapport largeur/hauteur. Les confondre laisserait passer un objet juste de
#     forme et deux fois trop haut, ou l'inverse.
#
# ⚠️ ET IL NE PARTAGE AUCUNE LIGNE AVEC L'APPLICATION. Il rasterise sur
#    processeur par `rendre_vues_corpus.py` (identite aux octets deja prouvee) ;
#    il ne connait ni NKRenderer, ni NkEditMesh, ni le moteur. Un instrument qui
#    juge un rendu avec le code de ce rendu s'accorde avec lui-meme.
#
# USAGE :
#   python fidelite_reference.py --zero <id corpus>
#   python fidelite_reference.py --reference <id> --candidat <a.obj> [--pas 5]
#   python fidelite_reference.py --jeu epreuve_fidelite.txt --candidats DOSSIER
# -----------------------------------------------------------------------------
import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rendre_vues_corpus as R

for _f in (sys.stdout, sys.stderr):
    try:
        _f.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

CORPUS = "logs_genia3d/corpus_vues"
# Le corpus tourne 16 azimuts x 3 elevations ; l'ordre est azimut-majeur et les
# elevations valent [-15, 15, 45]. Les trois vues de l'epreuve sont donc :
IDX_VUES = (1, 13, 25)     # azimuts 0, 90 et 180 degres, elevation +15
AZ_VUES = (0.0, 90.0, 180.0)
ELEVATION = 15.0
RAYON, FOV = 2.6, 40.0


def masque_reference(ident, dossier=CORPUS):
    """Les trois silhouettes de la reference. L'ALPHA DU RENDU EST LA
    SILHOUETTE : aucun detourage ne s'interpose, donc on ne mesure pas la
    qualite d'un detoureur en croyant mesurer la notre."""
    z = np.load(os.path.join(dossier, ident + ".npz"))
    a = z["rgba"][list(IDX_VUES), ..., 3]
    poses = z["poses"][list(IDX_VUES)]
    for k, p in enumerate(poses):   # la pose ECRITE doit etre celle qu'on croit
        if abs(float(p[0]) - AZ_VUES[k]) > 1e-6 or abs(float(p[1]) - ELEVATION) > 1e-6:
            raise ValueError("pose inattendue pour %s vue %d : %s" % (ident, k, p))
    return a > 127


def masques_candidat(chemin, decalage, taille):
    """Les trois silhouettes du candidat, vu sous `decalage` degres d'azimut."""
    Vt, F = R.charger(chemin)
    out = []
    for az in AZ_VUES:
        p = R.pose_depuis_angles(az + decalage, ELEVATION, RAYON, (0, 0, 0), FOV,
                                 taille, taille, 0.05, 10.0)
        _, msk, _ = R.rendre_vectorise(Vt, F, p)
        out.append(msk)
    return out


def recadrer(m, n=128):
    """Ramene le masque a sa boite englobante puis a n x n. C'est ce qui rend
    C_forme AVEUGLE A LA TAILLE -- et c'est voulu : C_prop mesure a part ce que
    cette normalisation retire."""
    ys, xs = np.nonzero(m)
    if not len(ys):
        return np.zeros((n, n), dtype=bool), 0.0
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    c = m[y0:y1, x0:x1]
    h, w = c.shape
    yi = (np.arange(n) * h // n).clip(0, h - 1)
    xi = (np.arange(n) * w // n).clip(0, w - 1)
    return c[yi][:, xi], float(w) / float(h)


def iou(a, b):
    u = np.logical_or(a, b).sum()
    return float(np.logical_and(a, b).sum()) / float(u) if u else 0.0


def comparer(mref, mcand):
    """(IoU moyen sur les trois vues, ecart moyen de ln(largeur/hauteur))."""
    ious, ecarts = [], []
    for r, c in zip(mref, mcand):
        rr, ar = recadrer(r)
        cc, ac = recadrer(c)
        ious.append(iou(rr, cc))
        if ar > 0 and ac > 0:
            ecarts.append(abs(np.log(ac) - np.log(ar)))
        else:
            ecarts.append(float("inf"))
    return float(np.mean(ious)), float(np.mean(ecarts))


def meilleur_decalage(mref, chemin, pas, taille):
    """LE DECALAGE EST COMMUN AUX TROIS VUES. On ne cherche pas le meilleur
    angle vue par vue : ce serait accorder au candidat trois objets differents."""
    best = (-1.0, 0.0, float("inf"))
    for d in np.arange(0.0, 360.0, pas):
        m = masques_candidat(chemin, float(d), taille)
        v, e = comparer(mref, m)
        if v > best[0]:
            best = (v, float(d), e)
    return best


def zero(ident, taille, dossier=CORPUS):
    """LE ZERO : la reference contre elle-meme. Doit rendre 1,000000."""
    mref = masque_reference(ident, dossier)
    v, e = comparer(mref, mref)
    ok = abs(v - 1.0) < 1e-12 and e < 1e-12
    print("ZERO %-34s IoU=%.6f ecart_prop=%.6f -> %s"
          % (ident, v, e, "vert" if ok else "HORS SERVICE"))
    return ok


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--corpus", default=CORPUS)
    p.add_argument("--zero")
    p.add_argument("--reference")
    p.add_argument("--candidat")
    p.add_argument("--jeu")
    p.add_argument("--candidats", help="dossier contenant <cle>.obj")
    p.add_argument("--pas", type=float, default=5.0)
    p.add_argument("--taille", type=int, default=384)
    p.add_argument("--negatif", action="store_true",
                   help="croise chaque reference avec les candidats des AUTRES")
    a = p.parse_args()

    if a.zero:
        sys.exit(0 if zero(a.zero, a.taille, a.corpus) else 3)

    if a.reference and a.candidat:
        if not zero(a.reference, a.taille, a.corpus):
            sys.exit(3)
        mref = masque_reference(a.reference, a.corpus)
        v, d, e = meilleur_decalage(mref, a.candidat, a.pas, a.taille)
        print("%-24s C_forme=%.4f  C_prop=%.4f  (decalage %.0f deg)"
              % (os.path.basename(a.candidat), v, e, d))
        sys.exit(0 if (v >= 0.70 and e <= 0.25) else 1)

    if a.jeu and a.candidats:
        jeu = []
        for l in open(a.jeu, encoding="utf-8"):
            l = l.strip()
            if not l or l.startswith("#"):
                continue
            c = [x.strip() for x in l.split("|")]
            if len(c) >= 4:
                jeu.append(dict(cle=c[0], ident=c[1], demande=c[2], famille=c[3]))
        # 1. LE ZERO D'ABORD, sur chaque reference.
        for it in jeu:
            if not zero(it["ident"], a.taille, a.corpus):
                print("\nINSTRUMENT HORS SERVICE : le zero a echoue.")
                sys.exit(3)
        print("")
        # 2. LA MATRICE : chaque reference contre chaque candidat present.
        # .obj (l'assemblage exporte par l'application) ou .glb (la voie image) :
        # le meme juge doit pouvoir peser les deux voies, sinon on les compare
        # avec deux instruments et l'ecart mesure les instruments.
        cands = {}
        for it in jeu:
            for ext in (".obj", ".glb"):
                ch = os.path.join(a.candidats, it["cle"] + ext)
                if os.path.exists(ch):
                    cands[it["cle"]] = ch
                    break
        if not cands:
            print("Aucun candidat .obj dans %s" % a.candidats)
            sys.exit(2)
        res = {}
        for it in jeu:
            mref = masque_reference(it["ident"], a.corpus)
            for cle, ch in cands.items():
                v, d, e = meilleur_decalage(mref, ch, a.pas, a.taille)
                res[(it["cle"], cle)] = (v, e, d)
        # 3. LE VERDICT. ⚠️ ON N'AFFICHE PAS QUE LE SEUIL : on affiche le RANG du
        #    bon candidat parmi tous. Un seuil dit « au-dessus de la barre » ; le
        #    rang dit si le critere DISCRIMINE, ce qui est une autre question et
        #    la seule qui compte pour « est-ce CET objet-la ».
        n = len(cands)
        print("%-10s %-9s %-9s %-9s %-9s %-9s %-9s %s" %
              ("reference", "C_forme", "neg_max", "marge", "C_prop", "neg_prop", "rangs", "verdict"))
        rouges, rang1_f, rang1_p = 0, 0, 0
        for it in jeu:
            k = it["cle"]
            if (k, k) not in res:
                continue
            v, e, d = res[(k, k)]
            autres_f = [res[(k, c)][0] for c in cands if c != k]
            autres_p = [res[(k, c)][1] for c in cands if c != k]
            neg = max(autres_f) if autres_f else 0.0
            negp = min(autres_p) if autres_p else float("inf")
            rf = 1 + sum(1 for x in autres_f if x > v)
            rp = 1 + sum(1 for x in autres_p if x < e)
            rang1_f += (rf == 1)
            rang1_p += (rp == 1)
            ok = (v >= 0.70) and (e <= 0.25) and (v > neg)
            rouges += 0 if ok else 1
            print("%-10s %-9.4f %-9.4f %-+9.4f %-9.4f %-9.4f %d/%d %d/%d  %s"
                  % (k, v, neg, v - neg, e, negp, rf, n, rp, n,
                     "vert" if ok else "ROUGE (forme %s, prop %s, bat le negatif %s)"
                     % ("ok" if v >= 0.70 else "NON", "ok" if e <= 0.25 else "NON",
                        "oui" if v > neg else "NON")))
        print("")
        print("%d ROUGE(S) sur %d" % (rouges, n))
        print("DISCRIMINATION : le bon candidat arrive PREMIER %d fois sur %d en FORME et "
              "%d fois sur %d en PROPORTIONS. Le hasard en donnerait 1." % (rang1_f, n, rang1_p, n))
        sys.exit(1 if rouges else 0)
    p.print_help()
    sys.exit(2)


if __name__ == "__main__":
    main()
