# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/epreuve_creation.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# LE JUGE DU JEU D'EPREUVE « une phrase -> un objet en parties nommees ».
#
# Il lit DEUX choses, et rien d'autre :
#   - le jeu d'epreuve (epreuve_creation.txt), ecrit AVANT le code ;
#   - le journal de mesure que NK3DModeler ecrit (logs/crea_mesure.txt), une
#     copie par demande : des BOITES MONDE relues a l'hote apres la pose.
# ⚠️ IL NE LIT PAS LE VERDICT DE L'APPLICATION. Il recalcule chaque critere sur
#    les boites. Un juge qui recopierait le « 0 flottante » du fil ne mesurerait
#    que la confiance de l'application en elle-meme.
#
# USAGE : python epreuve_creation.py --jeu epreuve_creation.txt --resultats <dossier>
#         (le dossier contient <cle>.txt, copie du journal de chaque course)
# -----------------------------------------------------------------------------
import argparse
import os
import re
import sys

# La console de Windows est en cp1252 : un juge qui meurt en imprimant son
# propre avertissement ne rend aucun verdict.
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

GENERIQUE = re.compile(r"^(partie|part|piece|objet|element|cube|cylindre|cone|sphere|tore|capsule|plan)[_0-9]*$")
TOL_SOL = 0.005
TOL_CONTACT = 0.01


def lire_jeu(chemin):
    jeu = []
    for ligne in open(chemin, encoding="utf-8"):
        ligne = ligne.strip()
        if not ligne or ligne.startswith("#"):
            continue
        c = [x.strip() for x in ligne.split("|")]
        if len(c) < 7:
            continue
        fourchette = lambda s: tuple(float(x.rstrip(".")) for x in s.split("-"))
        jeu.append(dict(cle=c[0], famille=c[1], demande=c[2], min_parties=int(c[3]),
                        H=fourchette(c[4]), L=fourchette(c[5]), l=fourchette(c[6])))
    return jeu


def lire_mesure(chemin):
    """Rend (parties du DERNIER lot pose, ligne d'annulation ou None)."""
    lots, annul = {}, None
    courant = None
    for ligne in open(chemin, encoding="utf-8", errors="replace"):
        m = ligne.split()
        if not m:
            continue
        if m[0] == "LOT":
            courant = int(m[1])
            lots[courant] = []
        elif m[0] == "PARTIE" and courant is not None:
            lots[int(m[1])].append((m[2], [float(x) for x in m[3:6]], [float(x) for x in m[6:9]]))
        elif m[0] == "ANNULATION":
            annul = dict(kv.split("=") for kv in m[2:])
    if not lots:
        return None, annul
    return lots[max(lots)], annul


def touche(a, b, tol):
    return all(a[1][k] <= b[2][k] + tol and b[1][k] <= a[2][k] + tol for k in range(3))


def juger(item, parties, annul):
    r = {}
    noms = [p[0] for p in parties]
    distincts = len(set(noms)) == len(noms)
    generiques = [n for n in noms if GENERIQUE.match(n.lower())]
    r["C1"] = len(noms) >= item["min_parties"] and distincts and not generiques
    r["C1_detail"] = "%d parties%s%s" % (len(noms), "" if distincts else ", NOMS EN DOUBLE",
                                          (", generiques : " + ",".join(generiques)) if generiques else "")
    mn = [min(p[1][k] for p in parties) for k in range(3)]
    mx = [max(p[2][k] for p in parties) for k in range(3)]
    r["C2"] = abs(mn[1]) <= TOL_SOL
    r["C2_detail"] = "bas a y=%.4f" % mn[1]
    # RIEN NE FLOTTE : parcours depuis les parties posees au sol.
    atteint = [abs(p[1][1] - mn[1]) <= TOL_CONTACT and abs(p[1][1]) <= TOL_CONTACT for p in parties]
    file = [i for i, a in enumerate(atteint) if a]
    while file:
        i = file.pop()
        for j in range(len(parties)):
            if not atteint[j] and touche(parties[i], parties[j], TOL_CONTACT):
                atteint[j] = True
                file.append(j)
    flott = [parties[i][0] for i, a in enumerate(atteint) if not a]
    r["C3"] = not flott
    r["C3_detail"] = "flottantes : " + (",".join(flott) if flott else "aucune")
    H = mx[1] - mn[1]
    ex, ez = mx[0] - mn[0], mx[2] - mn[2]
    L, l = max(ex, ez), min(ex, ez)
    dans = lambda v, f: f[0] <= v <= f[1]
    r["C4"] = dans(H, item["H"]) and dans(L, item["L"]) and dans(l, item["l"])
    r["C4_detail"] = "H=%.2f%s L=%.2f%s l=%.2f%s" % (H, "" if dans(H, item["H"]) else "(!)", L,
                                                    "" if dans(L, item["L"]) else "(!)", l,
                                                    "" if dans(l, item["l"]) else "(!)")
    if annul is None:
        r["C5"] = False
        r["C5_detail"] = "pas de mesure d'annulation"
    else:
        r["C5"] = (annul.get("geste") == "1" and annul.get("objets_apres") == annul.get("objets_avant")
                   and annul.get("restants") == "0")
        r["C5_detail"] = "geste=%s objets %s->%s restants=%s" % (annul.get("geste"), annul.get("objets_avant"),
                                                               annul.get("objets_apres"), annul.get("restants"))
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jeu", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "epreuve_creation.txt"))
    ap.add_argument("--resultats", required=True)
    a = ap.parse_args()
    jeu = lire_jeu(a.jeu)
    tot = {k: 0 for k in ("C1", "C2", "C3", "C4", "C5")}
    complets = 0
    print("%-11s | %-3s %-3s %-3s %-3s %-3s | detail" % ("cle", "C1", "C2", "C3", "C4", "C5"))
    for item in jeu:
        chemin = os.path.join(a.resultats, item["cle"] + ".txt")
        if not os.path.isfile(chemin):
            print("%-11s | AUCUNE MESURE (%s absent)" % (item["cle"], chemin))
            continue
        parties, annul = lire_mesure(chemin)
        if not parties:
            print("%-11s | AUCUN LOT POSE" % item["cle"])
            continue
        r = juger(item, parties, annul)
        ok = [r[k] for k in ("C1", "C2", "C3", "C4", "C5")]
        for k in tot:
            tot[k] += 1 if r[k] else 0
        complets += 1 if all(ok) else 0
        print("%-11s | %-3s %-3s %-3s %-3s %-3s | %s ; %s ; %s ; %s ; %s" % (
            item["cle"], *["oui" if x else "NON" for x in ok], r["C1_detail"], r["C2_detail"],
            r["C3_detail"], r["C4_detail"], r["C5_detail"]))
    n = len(jeu)
    print()
    for k in ("C1", "C2", "C3", "C4", "C5"):
        print("  %s : %d / %d" % (k, tot[k], n))
    print("  les cinq a la fois : %d / %d" % (complets, n))
    print("  ⚠ aucun critere ne juge la RESSEMBLANCE : ouvrir les images.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
