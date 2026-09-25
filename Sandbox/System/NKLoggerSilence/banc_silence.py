#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# Sandbox/System/NKLoggerSilence/banc_silence.py
#
# BANC « la console reste muette ».
#
# Il lance NKLoggerSilence.exe dans trois conditions, RELIT ce qui est sorti
# (jamais ce qu'on croit avoir vu), et rend un verdict par critere.
#
# ⚠️ IL A UN NEGATIF, ET C'EST LA MOITIE DE SA VALEUR. Lance sur le binaire
#    d'AVANT le correctif du 2026-09-25, il DOIT rougir sur C1, C2 et C4. Un
#    banc qui reste vert des deux cotes ne mesure rien :
#        python banc_silence.py --exe <...>/NKLoggerSilence_AVANT.exe --negatif
#
# ⚠️ ON NE JUGE PAS SUR LE CODE DE SORTIE DU PROCESSUS. On juge sur les
#    marqueurs relus dans stdout, stderr et logs/app.log.
#
# Usage :
#   python banc_silence.py [--exe CHEMIN] [--negatif]
# =============================================================================

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

DEFAUT_EXE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "..",
    "Build", "Bin", "Release-Windows", "NKLoggerSilence", "NKLoggerSilence.exe")


def lancer(exe, dossier, variables):
    """Lance la sonde dans un dossier neuf ; rend (stdout, stderr, journal)."""
    if os.path.isdir(dossier):
        shutil.rmtree(dossier, ignore_errors=True)
    os.makedirs(dossier, exist_ok=True)

    env = dict(os.environ)
    # On part d'un environnement PROPRE : une variable laissee par une mesure
    # precedente fausserait la suivante sans rien dire.
    for cle in ("NK_LOG_LEVEL", "NK_LOG_CONSOLE", "NK_LOG_QUIET", "NK_LOG_BANNER"):
        env.pop(cle, None)
    env.update(variables)

    sortie = os.path.join(dossier, "stdout.txt")
    erreur = os.path.join(dossier, "stderr.txt")
    with open(sortie, "wb") as fo, open(erreur, "wb") as fe:
        subprocess.run([exe], cwd=dossier, env=env, stdout=fo, stderr=fe, timeout=120)

    def lire(chemin):
        try:
            with open(chemin, "r", encoding="utf-8", errors="replace") as f:
                return f.read()
        except OSError:
            return ""

    return lire(sortie), lire(erreur), lire(os.path.join(dossier, "logs", "app.log"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=os.path.normpath(DEFAUT_EXE))
    ap.add_argument("--negatif", action="store_true",
                    help="inverse le verdict : on ATTEND que C1, C2 et C4 rougissent")
    args = ap.parse_args()

    if not os.path.isfile(args.exe):
        print("ECHEC : binaire introuvable : %s" % args.exe)
        return 2

    base = tempfile.mkdtemp(prefix="banc_nklogger_")
    try:
        # --- condition 1 : rien de regle, sortie REDIRIGEE ------------------
        o1, e1, j1 = lancer(args.exe, os.path.join(base, "defaut"), {})
        # --- condition 2 : NK_LOG_LEVEL=debug, une seule variable -----------
        o2, e2, j2 = lancer(args.exe, os.path.join(base, "debug"), {"NK_LOG_LEVEL": "debug"})
        # --- condition 3 : NK_LOG_CONSOLE=0, l'explicite l'emporte ----------
        o3, e3, j3 = lancer(args.exe, os.path.join(base, "sansconsole"),
                            {"NK_LOG_LEVEL": "debug", "NK_LOG_CONSOLE": "0"})

        criteres = []

        # C1 : un puits console EXISTE en Release (il n'existait pas avant).
        criteres.append(("C1 puits console branche en Release (GetSinkCount=3)",
                         "GetSinkCount=3" in o1))

        # C2 : une ERREUR se voit hors du fichier. Sur stderr, donc sans
        #      toucher stdout que lisent les bancs du depot.
        criteres.append(("C2 logger.Error() visible sur stderr",
                         "[NKLOG] C14 error" in e1))

        # C3 : stdout n'a RIEN gagne. C'est la garde anti-bavardage : si une
        #      ligne de journal apparaissait sur la sortie standard, tous les
        #      bancs du depot qui la lisent changeraient de resultat.
        criteres.append(("C3 stdout ne recoit aucune ligne de journal",
                         "[NKLOG]" not in o1))

        # C4 : UNE SEULE variable suffit pour voir un Debug, a l'ecran ET dans
        #      le fichier -- le fichier est ce qu'on envoie quand on demande de
        #      l'aide, il ne doit pas rester en retard sur l'ecran.
        criteres.append(("C4 NK_LOG_LEVEL=debug : le Debug sort (hors fichier)",
                         "[NKLOG] C11 debug" in (o2 + e2)))
        criteres.append(("C4bis NK_LOG_LEVEL=debug : le Debug entre dans logs/app.log",
                         "[NKLOG] C11 debug" in j2))

        # C5 : la banniere ne parait PAS quand la sortie est redirigee.
        criteres.append(("C5 aucune banniere quand stderr n'est pas une console",
                         "[NKLogger] niveau=" not in (o1 + e1)))

        # C6 : NK_LOG_CONSOLE=0 retire vraiment le puits, et le fichier continue.
        criteres.append(("C6 NK_LOG_CONSOLE=0 retire le puits (GetSinkCount=2)",
                         "GetSinkCount=2" in o3))
        criteres.append(("C6bis ... et le journal fichier recoit quand meme le Debug",
                         "[NKLOG] C11 debug" in j3))

        # C7 : la seconde porte du module. Elle ne branche AUCUN puits, et ce
        #      n'est pas le correctif qui le change : le critere est la pour
        #      que la table ne mente pas si quelqu'un modifie le registre.
        criteres.append(("C7 CreateLogger() rend un logger SANS puits (constat, non corrige)",
                         "CreateLogger: GetSinkCount=0" in o1))

        print("Binaire : %s" % args.exe)
        print("")
        rouges = []
        for nom, vert in criteres:
            print("  [%s] %s" % ("OK " if vert else "NON", nom))
            if not vert:
                rouges.append(nom)
        print("")

        if args.negatif:
            # Le negatif attend precisement les trois criteres que le correctif
            # a fait passer. Les nommer un par un evite qu'un banc casse pour
            # une autre raison soit pris pour une refutation reussie.
            attendus_rouges = [n for n in rouges
                               if n.startswith("C1 ") or n.startswith("C2 ") or n.startswith("C4 ")]
            if len(attendus_rouges) == 3:
                print("NEGATIF CONFORME : C1, C2 et C4 rougissent sur le binaire d'avant.")
                return 0
            print("NEGATIF NON CONFORME : attendu C1+C2+C4 rouges, obtenu %s" % attendus_rouges)
            return 1

        if rouges:
            print("VERDICT : ROUGE (%d critere(s))" % len(rouges))
            return 1
        print("VERDICT : VERT (%d criteres)" % len(criteres))
        return 0
    finally:
        shutil.rmtree(base, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
