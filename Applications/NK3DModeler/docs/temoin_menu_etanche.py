# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# TEMOIN : « UN MENU DE LA BARRE PRINCIPALE NE LAISSE RIEN PASSER »
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# Le defaut, ses mots (2026-09-05) : « les menus du menu principal laissent
# traverser les evenements ». Meme famille que deux defauts deja clos ailleurs :
# la molette sous un menu contextuel, et les modales qui laissaient passer souris
# ET clavier.
#
# La cause, mesuree
# -----------------
#   * `st.openMenu >= 0` n'entrait PAS dans la condition qui vide l'entree a la
#     source (main.cpp) : molette, touches et caracteres arrivaient donc aux
#     panneaux comme si de rien n'etait ;
#   * `PaintOpenMenu` ne declarait AUCUNE emprise (`UiBlockAdd`). Les six menus
#     contextuels de la hierarchie la declarent depuis le 14 aout ; celui de la
#     barre principale n'y avait jamais ete rattache -- une lecon ecrite a cote
#     d'un chemin ne couvre pas le chemin voisin.
#
# ⚠️ CE QUE CE TEMOIN PROUVE, ET CE QU'IL NE PROUVE PAS
# ----------------------------------------------------
# AUCUNE injection d'entree souris ni clavier n'est permise ici. Ce temoin ne
# fabrique donc NI molette NI touche. Il mesure, sur le chemin reel de la boucle :
#   1. `reserve` — un drapeau pose DANS la branche qui vide l'entree, jamais
#      recalcule a cote : il dit que le vidage a eu lieu, pas qu'on le voulait ;
#   2. `emprise` — le rectangle que les panneaux consultent reellement, apres
#      basculement, et s'il couvre la vue 3D et la hierarchie.
# Il NE prouve PAS qu'un vrai coup de molette est avale : ça reste un geste
# humain a faire. C'est dit ici plutot que sous-entendu.
#
# Il PEUT rougir, deux fois, sur les deux causes separement :
#   `--mutation` retire `menuDeroule` de la condition de vidage (reserve -> 0),
#   puis retire `UiBlockAdd(box)` de `PaintOpenMenu` (emprise -> 0). Restauration
#   DEPUIS UNE COPIE dans les deux cas.
#
# Un CONTROLE NEGATIF est joue a chaque fois : la meme course SANS menu ouvert
# doit rendre reserve=0 et emprise=0. Sans lui, un temoin qui lit « 1 » partout
# applaudirait une sonde bloquee sur vrai.
# -----------------------------------------------------------------------------
import io
import os
import re
import shutil
import subprocess
import sys

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
EXE = os.path.join(RACINE, "Build", "Bin", "Release-Windows", "NK3DModeler", "NK3DModeler.exe")
SRC = os.path.join(RACINE, "Applications", "NK3DModeler", "src", "NK3DModeler")
MAIN = os.path.join(SRC, "main.cpp")
MENUS = os.path.join(SRC, "Shell", "NkModelerMenus.h")

MUT_VIDAGE = ("		if (modalOpen || menuDeroule || sourisSurJournal) {\n			saisieVidee = true;",
              "		if (modalOpen || sourisSurJournal) { // MUTATION DU TEMOIN\n			saisieVidee = true;")
MUT_EMPRISE = ("			st.UiBlockAdd(box);",
               "			// MUTATION DU TEMOIN : aucune emprise declaree")


def ilyana():
    out = subprocess.run(["tasklist"], capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if "NKIlyana" in l]


def course(menu):
    env = dict(os.environ)
    env["NK_OPEN_RECENT"] = "0"
    env["NK_MENU_PROBE"] = "40"
    env["NK_AGENT_EXIT"] = "60"
    if menu is not None:
        env["NK_MENU_OPEN"] = str(menu)
    else:
        env.pop("NK_MENU_OPEN", None)
    r = subprocess.run([EXE], cwd=RACINE, env=env, capture_output=True, timeout=240)
    s = (r.stdout or b"").decode("utf-8", "replace")
    m = re.search(r"\[sonde3\] menu=(-?\d+) reserve=(\d) emprise=(\d) "
                  r"\((-?\d+), (-?\d+), (-?\d+), (-?\d+)\) "
                  r"couvre_vue=(\d) couvre_hier=(\d) couvre_navig=(\d)", s)
    if not m:
        return None
    g = [int(x) for x in m.groups()]
    return dict(menu=g[0], reserve=g[1], emprise=g[2], rect=tuple(g[3:7]),
                vue=g[7], hier=g[8], navig=g[9])


def construire():
    r = subprocess.run("jenga build --target NK3DModeler --config Release",
                       cwd=RACINE, capture_output=True, timeout=900, shell=True)
    s = (r.stdout or b"").decode("utf-8", "replace") + (r.stderr or b"").decode("utf-8", "replace")
    return "SUCCESS" in s


def muter(fichier, avant, apres):
    src = io.open(fichier, encoding="utf-8").read()
    io.open(fichier + ".avant-mutation", "w", encoding="utf-8", newline="").write(src)
    assert avant in src, "point de mutation introuvable dans " + fichier
    io.open(fichier, "w", encoding="utf-8", newline="").write(src.replace(avant, apres, 1))


def restaurer(fichier):
    shutil.copyfile(fichier + ".avant-mutation", fichier)
    os.remove(fichier + ".avant-mutation")


def dire(nom, r):
    if r is None:
        print("   %-14s SONDE MUETTE" % nom)
        return
    print("   %-14s menu=%d reserve=%d emprise=%d %s couvre_vue=%d couvre_hier=%d"
          % (nom, r["menu"], r["reserve"], r["emprise"], r["rect"], r["vue"], r["hier"]))


def main():
    mute = "--mutation" in sys.argv
    print("Ilyana avant :", ilyana() or "ABSENTE")

    print("\n-- controle negatif : aucun menu ouvert --")
    ferme = course(None)
    dire("ferme", ferme)
    ok_ferme = ferme is not None and ferme["reserve"] == 0 and ferme["emprise"] == 0

    print("\n-- mesure : le menu Fichier deroule --")
    ouvert = course(0)
    dire("ouvert", ouvert)
    ok_ouvert = (ouvert is not None and ouvert["reserve"] == 1 and ouvert["emprise"] == 1
                 and ouvert["vue"] == 1 and ouvert["hier"] == 1)

    verdict = ok_ferme and ok_ouvert
    print("\nVERDICT : %s"
          % ("VERT -- la saisie est reservee ET l'emprise couvre le dessous"
             if verdict else "ROUGE"))

    if not mute:
        print("\nIlyana apres :", ilyana() or "ABSENTE")
        return 0 if verdict else 1

    rouges = []
    for nom, fichier, (a, b) in (("vidage", MAIN, MUT_VIDAGE), ("emprise", MENUS, MUT_EMPRISE)):
        print("\n== MUTATION « %s » ==" % nom)
        muter(fichier, a, b)
        try:
            if not construire():
                print("   build mute : ECHEC")
                rouges.append(False)
                continue
            r = course(0)
            dire("mute/" + nom, r)
            if nom == "vidage":
                rouge = r is None or r["reserve"] == 0
            else:
                rouge = r is None or r["emprise"] == 0 or (r["vue"] == 0 and r["hier"] == 0)
            rouges.append(rouge)
            print("   MUTATION : %s"
                  % ("ROUGE comme attendu" if rouge else "VERTE -- LE TEMOIN NE MESURE RIEN"))
        finally:
            restaurer(fichier)
    construire()
    print("   fichiers restaures depuis leurs copies, binaire reconstruit")
    print("\nIlyana apres :", ilyana() or "ABSENTE")
    return 0 if (verdict and all(rouges) and len(rouges) == 2) else 1


if __name__ == "__main__":
    sys.exit(main())
