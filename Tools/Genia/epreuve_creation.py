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
    """Rend (parties du DERNIER lot pose, ligne d'annulation, ligne MAISON)."""
    lots, annul, maison = {}, None, None
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
        elif m[0] == "MAISON":
            maison = dict(kv.split("=", 1) for kv in m[1:] if "=" in kv)
    if not lots:
        return None, annul, maison
    return lots[max(lots)], annul, maison


def touche(a, b, tol):
    return all(a[1][k] <= b[2][k] + tol and b[1][k] <= a[2][k] + tol for k in range(3))


def juger(item, parties, annul, maison=None):
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
    # C6 (21/09, Q7) COHERENCE D'ECHELLE : aucune partie dont le plus grand cote
    # depasse 10 fois la MEDIANE des plus grands cotes des autres parties -- le
    # « mur geant » a cote d'un verre de 10 cm. Meme seuil que la garde de
    # l'application ; ici il se recalcule sur les boites POSEES, jamais sur le
    # verdict de l'application. Une seule partie : vrai par definition.
    ext = [max(p[2][k] - p[1][k] for k in range(3)) for p in parties]
    hors = []
    for i, e in enumerate(ext):
        autres = sorted(ext[:i] + ext[i + 1:])
        if not autres:
            continue
        m = len(autres)
        med = autres[m // 2] if m % 2 else 0.5 * (autres[m // 2 - 1] + autres[m // 2])
        if med > 1e-5 and e > 10.0 * med * 1.001:
            hors.append("%s (%.0fx)" % (parties[i][0], e / med))
    r["C6"] = not hors
    r["C6_detail"] = "hors d'echelle : " + (",".join(hors) if hors else "aucune")

    # C7 (21/09, Q9.2) VRAISEMBLANCE D'UNE MAISON. NE S'APPLIQUE QU'AUX MAISONS,
    # et c'est volontaire : un critere qui rend « oui » pour un verre gonflerait
    # le total sans rien mesurer. Hors maison il vaut None, et le total le compte
    # a part -- « 3 / 3 maisons » et non « 8 / 8 objets ».
    #
    # Trois questions, toutes tirees des MESURES et non de l'oeil :
    #   (a) la hauteur d'etage : totale / niveaux, entre 2,4 m et 3,5 m. En
    #       dessous on ne passe pas debout, au-dessus ce n'est plus une maison ;
    #   (b) l'aplomb : le plus petit cote au sol contre la hauteur totale. Une
    #       villa n'est pas une tour ; en dessous de 0,7 c'en est une ;
    #   (c) un toit : une partie dont le nom le dit. Une boite sans toit reste
    #       une boite, et les cinq premiers criteres ne s'en apercoivent pas.
    if maison is None:
        r["C7"] = None
        r["C7_detail"] = "pas une maison (aucune ligne MAISON)"
    else:
        mn = [min(p[1][k] for p in parties) for k in range(3)]
        mx = [max(p[2][k] for p in parties) for k in range(3)]
        H = mx[1] - mn[1]
        cote = min(mx[0] - mn[0], mx[2] - mn[2])
        try:
            niv = max(1, int(maison.get("niveaux", "1")))
        except ValueError:
            niv = 1
        # ⚠️ LA HAUTEUR D'ETAGE SE MESURE SUR LES MURS, PAS SUR LA BOITE. Premiere
        #    version : hauteur totale / niveaux. Elle declarait « 6,14 m par etage »
        #    pour une maison d'un niveau dont le TOIT faisait 3,2 m -- le critere
        #    rougissait sur le toit en pretendant parler des murs. On prend donc le
        #    haut des pieces de MACONNERIE, et le toit se juge separement : il doit
        #    exister ET se trouver AU-DESSUS d'elles.
        murs = [p for p in parties if MUR.search(p[0].lower())]
        hmur = max((p[2][1] for p in murs), default=0.0)
        etage = hmur / niv if niv and hmur > 0.0 else 0.0
        aplomb = cote / H if H > 1e-6 else 0.0
        toit = [p for p in parties if TOIT.search(p[0].lower())]
        htoit = max((p[2][1] for p in toit), default=0.0)
        ok_a = 2.4 <= etage <= 3.5
        ok_b = 0.7 <= aplomb <= 8.0
        ok_c = bool(toit) and htoit > hmur + 0.05
        r["C7"] = ok_a and ok_b and ok_c
        r["C7_detail"] = "murs=%.2f m /%d niv -> etage=%.2f (%s) aplomb=%.2f (%s) toit=%s%s" % (
            hmur, niv, etage, "ok" if ok_a else "HORS 2.4-3.5", aplomb, "ok" if ok_b else "HORS 0.7-8",
            (",".join(p[0] for p in toit) if toit else "AUCUN"),
            "" if ok_c else (" (pas au-dessus des murs : %.2f)" % htoit if toit else ""))
    return r


# Les noms que NOS constructeurs donnent aux pieces de couverture (Q8/Q9) plus
# ceux qu'un modele ecrit spontanement. La liste est ECRITE, pas devinee : un
# critere qui accepterait n'importe quel nom ne mesurerait rien.
TOIT = re.compile(r"toit|versant|faitage|fa\u00eetage|tuile|acrotere|acrot\u00e8re|fronton|pignon|charpente|couverture|combles?|terrasse")

# La MACONNERIE : ce qui porte les etages. Liste ECRITE, comme celle du toit.
MUR = re.compile(r"mur|facade|façade|pignon|paroi|cloison|soubassement|etage|étage|niveau|corps|batiment|bâtiment")

CRIT = ("C1", "C2", "C3", "C4", "C5", "C6")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jeu", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "epreuve_creation.txt"))
    ap.add_argument("--resultats", required=True)
    a = ap.parse_args()
    jeu = lire_jeu(a.jeu)
    tot = {k: 0 for k in CRIT}
    complets = complets6 = 0
    maisons = maisons_ok = 0
    print("%-11s | %-3s %-3s %-3s %-3s %-3s %-3s %-3s | detail" % (("cle",) + CRIT + ("C7",)))
    for item in jeu:
        chemin = os.path.join(a.resultats, item["cle"] + ".txt")
        if not os.path.isfile(chemin):
            print("%-11s | AUCUNE MESURE (%s absent)" % (item["cle"], chemin))
            continue
        parties, annul, maison = lire_mesure(chemin)
        if not parties:
            print("%-11s | AUCUN LOT POSE" % item["cle"])
            continue
        r = juger(item, parties, annul, maison)
        ok = [r[k] for k in CRIT]
        for k in tot:
            tot[k] += 1 if r[k] else 0
        complets += 1 if all(ok[:5]) else 0
        complets6 += 1 if all(ok) else 0
        if r["C7"] is not None:
            maisons += 1
            maisons_ok += 1 if r["C7"] else 0
        c7 = "s.o" if r["C7"] is None else ("oui" if r["C7"] else "NON")
        print("%-11s | %-3s %-3s %-3s %-3s %-3s %-3s %-3s | %s ; %s ; %s ; %s ; %s ; %s ; %s" % (
            item["cle"], *["oui" if x else "NON" for x in ok], c7, r["C1_detail"], r["C2_detail"],
            r["C3_detail"], r["C4_detail"], r["C5_detail"], r["C6_detail"], r["C7_detail"]))
    n = len(jeu)
    print()
    for k in CRIT:
        print("  %s : %d / %d" % (k, tot[k], n))
    print("  les cinq a la fois : %d / %d" % (complets, n))
    print("  les six a la fois (avec l'echelle, C6) : %d / %d" % (complets6, n))
    print("  C7 vraisemblance de maison : %d / %d maison(s) -- les autres objets ne sont pas juges" % (
        maisons_ok, maisons))
    print("  ⚠ aucun critere ne juge la RESSEMBLANCE : ouvrir les images.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
