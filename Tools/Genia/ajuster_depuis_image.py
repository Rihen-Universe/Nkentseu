# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/ajuster_depuis_image.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# L'AJUSTEMENT PAR PROJECTION, TEL QUE L'APPLICATION L'APPELLE (Q17 dans NK3DModeler).
#
# Rodolf : « il faut que ca passe, car je veux deja tester la PERTINENCE du
# systeme de generation 3D. » L'ajustement vivait dans un outil de mesure ; il
# entre ici dans la chaine, appele par la MEME porte que TripoSR (un gabarit de
# commande, un fichier en entree, un fichier en sortie).
#
# CE QU'IL FAIT : une image (une vue, ou une PLANCHE multi-vues) + un nom de
# famille -> les parametres qui font coincider la silhouette de la famille avec
# celle de l'image, ecrits dans un petit fichier que le C++ relit.
#
# ⚠️ LES BORNES SONT CELLES DE VRAISEMBLANCE, ET ELLES SONT LUES DANS LE .nkfam.
#    Avec les bornes de CONSTRUCTION, l'ajustement inventait une porte de 8,95 m
#    pour gagner quelques pixels et PERDAIT sur le controle (-0,63) ; avec celles
#    de vraisemblance il GAGNE (+0,13). Et les lire dans le fichier evite la
#    recopie que j'avais laissee en dette dans `ajuster_famille.py`.
#
# ⚠️ UN SEUL PROCESSUS POUR TOUT LE BALAYAGE. Un processus par candidat coute
#    ~0,2 s de LANCEMENT, soit plus que tout le reste : dans un outil de mesure
#    on s'en moque, dans l'application non. `--famille-balayage` construit les N
#    candidats d'un coup (3 candidats en 88 ms, mesure).
#
# ⚠️ ET IL REFUSE EN LE DISANT. Objet presque plan avec une seule vue : une vue
#    3/4 ne determine pas ses parametres (mesure du 25/09 : 4 pertes sur 6 sous
#    le seuil de planeite). Le refus est ecrit dans le fichier de sortie, et
#    l'appelant pose la famille NON ajustee -- jamais rien.
#
# SORTIE (fichier texte, une cle par ligne) :
#   etat=ok|refus
#   motif=<texte>            (si refus)
#   vues=<n>
#   largeur=<m> hauteur=<m> profondeur=<m>
#   cfid=<distance obtenue>  secondes=<duree>
#
# USAGE : python ajuster_depuis_image.py --famille table --image X.png --sortie p.txt
# -----------------------------------------------------------------------------
import argparse
import itertools
import os
import subprocess
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import decouper_planche as DP
import fidelite_reference as FR
import rendre_vues_corpus as R

for _f in (sys.stdout, sys.stderr):
    try:
        _f.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

HARNESS = "Build/Bin/Release-Windows/NKEditMeshHarness/NKEditMeshHarness.exe"
SEUIL_PLANEITE = 0.08          # mesure du 25/09, cf. ajuster_famille.py
AZ = (0.0, 45.0, 90.0, 135.0)  # les azimuts essayes pour la premiere vue


def bornes_plausibles(famille, dossier="data/familles"):
    """Les bornes de VRAISEMBLANCE, LUES dans le .nkfam -- pas recopiees ici."""
    ch = os.path.join(dossier, famille + ".nkfam")
    out = {}
    if not os.path.exists(ch):
        return out
    for l in open(ch, encoding="utf-8", errors="replace"):
        m = l.split()
        if len(m) >= 5 and m[0] == "plausible":
            out[m[1]] = (float(m[2]), float(m[3]), float(m[4]))
    return out


def silhouettes(chemin):
    """Les silhouettes de l'image : plusieurs si c'est une planche, une sinon."""
    im, vues, _ = DP.analyser(chemin)
    m = DP.sujet(im)
    if len(vues) <= 1:
        return [m]
    out = []
    for b in vues:
        out.append(m[b["y0"]:b["y1"], b["x0"]:b["x1"]])
    return out


def rendre(chemin_obj, azimuts, taille=256):
    Vt, F = R.charger(chemin_obj)
    o = []
    for az in azimuts:
        p = R.pose_depuis_angles(az, FR.ELEVATION, FR.RAYON, (0, 0, 0), FR.FOV,
                                 taille, taille, 0.05, 10.0)
        _, msk, _ = R.rendre_vectorise(Vt, F, p)
        o.append(msk)
    return o


def cout(mref, mcand):
    """d_prop + (1 - IoU des silhouettes RECADREES).

    ⚠️ CE N'EST PAS EXACTEMENT LE C_fid DU JUGE, ET LA DIFFERENCE SE DIT. Le juge
       compare deux objets ramenes a la MEME boite 3D : l'IoU brut y a un sens.
       Ici la reference est une IMAGE de l'utilisateur -- aucune echelle commune,
       aucun cadrage commun, et souvent aucune resolution commune (512 contre
       256 a la premiere course). L'IoU brut n'existe donc pas, et le prendre
       quand meme comparerait des cadrages. On garde ce qui a un sens : la FORME
       (masques recadres a leur boite) et les PROPORTIONS (d_prop), c'est-a-dire
       les deux grandeurs que le controle negatif avait elues."""
    ious, ecarts = [], []
    for r, c in zip(mref, mcand):
        rr, ar = FR.recadrer(r)
        cc, ac = FR.recadrer(c)
        ious.append(FR.iou(rr, cc))
        ecarts.append(abs(np.log(max(ac, 1e-6)) - np.log(max(ar, 1e-6))))
    return float(np.mean(ecarts)) + (1.0 - float(np.mean(ious)))


def planeite(chemin_obj):
    Vt, F = R.charger(chemin_obj)
    e = Vt.max(axis=0) - Vt.min(axis=0)
    return float(e.min() / max(e.max(), 1e-9))


def balayer(famille, tuples, dossier):
    os.makedirs(dossier, exist_ok=True)
    ch = os.path.join(dossier, "tuples.txt")
    with open(ch, "w", encoding="utf-8") as f:
        for t in tuples:
            f.write("%.5f %.5f %.5f %d\n" % (t[0], t[1], t[2], int(t[3])))
    r = subprocess.run([os.path.abspath(HARNESS), "--famille-balayage", ch, "--nom", famille,
                        "--sortie-dir", dossier], capture_output=True, text=True)
    return r.returncode == 0


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--famille", required=True)
    p.add_argument("--image", required=True)
    p.add_argument("--sortie", required=True)
    p.add_argument("--pas", type=int, default=3, help="points par axe et par passe")
    p.add_argument("--travail", default="logs_genia3d/crea/ajust")
    a = p.parse_args()
    t0 = time.time()

    def ecrire(**kv):
        with open(a.sortie, "w", encoding="utf-8") as f:
            for k, v in kv.items():
                f.write("%s=%s\n" % (k, v))
        print("AJUST " + " ".join("%s=%s" % (k, v) for k, v in kv.items()))
        sys.stdout.flush()

    dom = bornes_plausibles(a.famille)
    if not dom:
        ecrire(etat="refus", motif="la famille « %s » ne declare aucune borne « plausible » "
                                   "dans son .nkfam : rien a ajuster" % a.famille,
               secondes="%.2f" % (time.time() - t0))
        return 0
    if not os.path.exists(a.image):
        ecrire(etat="refus", motif="image introuvable : %s" % a.image,
               secondes="%.2f" % (time.time() - t0))
        return 0

    mref = silhouettes(a.image)
    nv = len(mref)
    axes = [k for k in ("largeur", "hauteur", "profondeur") if k in dom]
    # ⚠️ UN AXE ABSENT DES BORNES N'EST PAS AJUSTE, il garde 0 (« au constructeur
    #    de decider »). Lui inventer un intervalle serait ajuster a l'aveugle.
    best = (None, float("inf"), 0.0)
    dm = {k: (dom[k][0], dom[k][1]) for k in axes}
    for passe in range(2):
        grille = [np.linspace(dm[k][0], dm[k][1], a.pas) for k in axes]
        tuples, cles = [], []
        for c in itertools.product(*grille):
            d = dict(zip(axes, c))
            tuples.append((d.get("largeur", 0.0), d.get("hauteur", 0.0), d.get("profondeur", 0.0), 0))
            cles.append(d)
        if not balayer(a.famille, tuples, a.travail):
            ecrire(etat="refus", motif="le banc n'a pas pu construire la famille (balayage)",
                   secondes="%.2f" % (time.time() - t0))
            return 0
        for i, d in enumerate(cles):
            ch = os.path.join(a.travail, "%03d.obj" % i)
            if not os.path.exists(ch):
                continue
            # l'angle : on essaie quelques azimuts pour la PREMIERE vue, et on
            # garde le meme decalage pour les suivantes -- un candidat qui
            # choisirait un angle par vue aurait trois objets differents.
            for az0 in AZ:
                azs = [az0 + 90.0 * k for k in range(nv)]
                c = cout(mref, rendre(ch, azs))
                if c < best[1]:
                    best = (d, c, az0)
        if best[0] is None:
            break
        dm = {k: (max(dom[k][0], v - (dom[k][1] - dom[k][0]) / (2.0 * (a.pas - 1))),
                  min(dom[k][1], v + (dom[k][1] - dom[k][0]) / (2.0 * (a.pas - 1))))
              for k, v in best[0].items()}

    if best[0] is None:
        ecrire(etat="refus", motif="aucun candidat n'a pu etre construit",
               secondes="%.2f" % (time.time() - t0))
        return 0

    # LE REFUS DE PLANEITE, avec son chiffre.
    tuples = [(best[0].get("largeur", 0.0), best[0].get("hauteur", 0.0), best[0].get("profondeur", 0.0), 0)]
    balayer(a.famille, tuples, a.travail)
    pl = planeite(os.path.join(a.travail, "000.obj"))
    if pl < SEUIL_PLANEITE and nv < 2:
        ecrire(etat="refus",
               motif="objet presque plan (planeite %.4f < %.2f) et UNE SEULE vue : "
                     "plusieurs jeux de parametres donnent la meme silhouette. "
                     "Joignez une planche multi-vues." % (pl, SEUIL_PLANEITE),
               vues=nv, secondes="%.2f" % (time.time() - t0))
        return 0

    ecrire(etat="ok", vues=nv, planeite="%.4f" % pl,
           largeur="%.4f" % best[0].get("largeur", 0.0),
           hauteur="%.4f" % best[0].get("hauteur", 0.0),
           profondeur="%.4f" % best[0].get("profondeur", 0.0),
           cfid="%.4f" % best[1], azimut="%.0f" % best[2],
           secondes="%.2f" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
