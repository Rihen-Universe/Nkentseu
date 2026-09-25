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

# LE SEUIL DE C_brut, ECRIT AVANT LA PREMIERE COURSE DU 25/09 (seconde passe).
# ⚠️ IL N'EST PAS DEDUIT DU PREMIER : le seuil de 0,70 sur C_forme a ete REFUTE
#    par son propre controle negatif. Celui-ci est pose a 0,50 parce qu'un
#    recouvrement de moitie entre deux silhouettes a la MEME echelle est le
#    minimum pour parler du meme objet -- et il sera juge, lui aussi, sur le
#    controle negatif : si les negatifs l'atteignent, c'est encore lui qui est
#    faux, et il faudra le dire.
SEUIL_BRUT = 0.50

# ── C_fid : LE CRITERE QUI SEPARE, ET IL A ETE CHERCHE, PAS DEVINE (25/09) ────
#     C_fid = d_prop + (1 - iou_brut)      -- une DISTANCE : 0 = identique.
# Deux criteres avaient ete REFUTES par leur propre controle negatif : C_forme
# (les negatifs montaient a 0,71-0,84) et C_brut seul (1 premier sur 6).
# `Tools/Genia/chercher_critere.py` a alors calcule sept grandeurs independantes
# sur les 36 couples de chaque jeu, et c'est le CONTROLE NEGATIF qui a elu
# celle-ci :
#     sur les maillages TripoSR (l'objet EST le bon)  : 6 premiers sur 6, marge
#         mediane +0,1735 ;
#     sur les assemblages (l'objet est generique)     : 2 sur 6, marge mediane
#         -0,4089.
# Autrement dit le critere separe quand il y a quelque chose a separer, et il
# refuse quand l'objet n'est pas celui de la reference. Aucune grandeur seule
# n'y arrivait : d_prop 5/6, iou_brut 5/6 ; c'est leur SOMME qui fait 6/6, et
# les deux mesurent des choses differentes (le rapport des cotes, et le
# recouvrement a echelle egale).
# ⚠️ IL A ETE CHOISI PARMI DIX COMBINAISONS SUR SIX OBJETS. C'est un choix fait
#    SUR le jeu qui le juge : il doit etre reprouve sur un jeu qu'il n'a pas vu
#    avant d'etre appele un critere. Les poids sont a 1 et 1, sans ajustement --
#    des coefficients regles auraient appris le jeu d'epreuve.
# ⚠️ ET AUCUN SEUIL ABSOLU N'EST INVENTE. Le verdict est RELATIF : battre le
#    meilleur des mauvais candidats. Un seuil pose ici serait ajuste sur six
#    objets, c'est-a-dire ajuste sur rien.


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
    """(C_forme, C_prop, C_brut) moyennes sur les trois vues.

    ⚠️ C_brut EST LE CRITERE QUI DECIDE, ET C_forme CELUI QUI M'A TROMPE.
    `recadrer` ramene chaque masque a SA boite englobante : deux silhouettes
    quelconques se recouvrent alors a 70-85 %, et le controle negatif franchissait
    le seuil (0,707 a 0,835 le 25/09). La renormalisation 2D DETRUIT precisement
    le signal qu'on cherche -- les proportions -- alors que le chargement a DEJA
    ramene la boite 3D a l'unite. C_brut compare donc les masques TELS QUELS,
    meme camera, meme echelle : une silhouette deux fois trop large y perd
    directement. C_forme et C_prop restent affiches, parce que leur DESACCORD dit
    quelque chose (forme juste + proportions fausses = un objet plausible qui
    n'est pas celui de la reference)."""
    ious, ecarts, bruts = [], [], []
    for r, c in zip(mref, mcand):
        rr, ar = recadrer(r)
        cc, ac = recadrer(c)
        ious.append(iou(rr, cc))
        bruts.append(iou(r, c))
        if ar > 0 and ac > 0:
            ecarts.append(abs(np.log(ac) - np.log(ar)))
        else:
            ecarts.append(float("inf"))
    return float(np.mean(ious)), float(np.mean(ecarts)), float(np.mean(bruts))


def meilleur_decalage(mref, chemin, pas, taille):
    """LE DECALAGE EST COMMUN AUX TROIS VUES. On ne cherche pas le meilleur
    angle vue par vue : ce serait accorder au candidat trois objets differents."""
    best = (-1.0, 0.0, float("inf"), -1.0)
    for d in np.arange(0.0, 360.0, pas):
        m = masques_candidat(chemin, float(d), taille)
        v, e, br = comparer(mref, m)
        # ⚠️ L'ANGLE EST CHOISI SUR C_brut : chercher l'angle qui maximise un
        #    critere puis juger avec un autre donnerait au candidat le meilleur
        #    des deux mondes.
        if br > best[3]:
            best = (v, float(d), e, br)
    return best


def zero(ident, taille, dossier=CORPUS):
    """LE ZERO : la reference contre elle-meme. Doit rendre 1,000000."""
    mref = masque_reference(ident, dossier)
    v, e, br = comparer(mref, mref)
    ok = abs(v - 1.0) < 1e-12 and e < 1e-12 and abs(br - 1.0) < 1e-12
    print("ZERO %-34s C_forme=%.6f C_brut=%.6f ecart_prop=%.6f -> %s"
          % (ident, v, br, e, "vert" if ok else "HORS SERVICE"))
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
        v, d, e, br = meilleur_decalage(mref, a.candidat, a.pas, a.taille)
        print("%-24s C_brut=%.4f  C_forme=%.4f  C_prop=%.4f  (decalage %.0f deg)"
              % (os.path.basename(a.candidat), br, v, e, d))
        sys.exit(0 if br >= SEUIL_BRUT else 1)
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
                v, d, e, br = meilleur_decalage(mref, ch, a.pas, a.taille)
                res[(it["cle"], cle)] = (v, e, d, br)
        # 3. LE VERDICT. C_fid tranche ; les autres grandeurs restent affichees
        #    parce que leur DESACCORD dit quelque chose (forme juste + proportions
        #    fausses = un objet plausible qui n'est pas celui de la reference).
        n = len(cands)
        print("%-10s %-9s %-9s %-9s %-9s %-9s %-11s %s" %
              ("reference", "C_fid", "neg_fid", "marge", "d_prop", "iou_brut", "rangs f/p/b", "verdict"))
        rouges, r1 = 0, [0, 0, 0]
        fid = lambda t: t[1] + (1.0 - t[3])      # d_prop + (1 - iou_brut)
        for it in jeu:
            k = it["cle"]
            if (k, k) not in res:
                continue
            t = res[(k, k)]
            v, e, d, br = t
            cf = fid(t)
            af = [fid(res[(k, c)]) for c in cands if c != k]
            ap = [res[(k, c)][1] for c in cands if c != k]
            ab = [res[(k, c)][3] for c in cands if c != k]
            negf = min(af) if af else float("inf")
            rf = 1 + sum(1 for x in af if x < cf)
            rp = 1 + sum(1 for x in ap if x < e)
            rb = 1 + sum(1 for x in ab if x > br)
            r1[0] += (rf == 1)
            r1[1] += (rp == 1)
            r1[2] += (rb == 1)
            ok = cf < negf
            rouges += 0 if ok else 1
            print("%-10s %-9.4f %-9.4f %-+9.4f %-9.4f %-9.4f %d/%d %d/%d %d/%d  %s"
                  % (k, cf, negf, negf - cf, e, br, rf, n, rp, n, rb, n,
                     "vert" if ok else "ROUGE (un AUTRE objet colle mieux a cette reference)"))
        print("")
        print("%d ROUGE(S) sur %d" % (rouges, n))
        print("DISCRIMINATION, le bon candidat arrive PREMIER : %d/%d en C_fid, "
              "%d/%d en d_prop, %d/%d en iou_brut. Le hasard en donnerait 1." %
              (r1[0], n, r1[1], n, r1[2], n))
        sys.exit(1 if rouges else 0)
    p.print_help()
    sys.exit(2)


if __name__ == "__main__":
    main()
