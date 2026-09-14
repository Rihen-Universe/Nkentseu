# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# TEMOIN : « UN PROJET DE PLUS DE 32 CARTES LES PORTE TOUTES »
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# Le defaut mesure le 2026-09-05
# ------------------------------
# Le projet `AgentTest` de Rodolf contient EXACTEMENT 32 cartes -- le plafond,
# plein. `NkModelerInput.h` declarait dix tableaux paralleles de 32
# (`browserKind[32]`, `browserNames[32][32]`...). Tout import y creait donc ses
# noeuds puis rendait :
#
#   [import] MESURE creation : model « Beta_Surface » ... carte=-1 fichier=(non ecrit)
#   [import] Import : 2 model(s), 2 maillage(s) - navigateur PLEIN, cartes partielles
#
# Reussi cote geometrie, INVISIBLE cote navigateur, AUCUN `.nkmesh` sur le
# disque. C'est la seconde moitie de ce que Rodolf appelait « l'import refuse ».
#
# Ce que ce temoin verifie
# ------------------------
#   1. l'import dans un projet DEJA a 32 cartes cree bien les cartes 32 et 33 ;
#   2. les fichiers `.nkmesh` correspondants existent sur le disque ;
#   3. un import de PLUS -- le projet passe alors a 36 cartes -- ne perd rien.
#
# Il PEUT rougir : `--mutation` remet un plafond de 32 dans `CardAdd`, seul
# endroit ou une carte nait desormais. Le temoin doit alors dire ROUGE. Le
# fichier est restaure DEPUIS UNE COPIE, jamais par une substitution inverse.
#
# ⚠️ IL MUTE UN PROJET REEL. Le projet est donc SAUVEGARDE avant, et RESTITUE
# apres, quoi qu'il arrive.
# -----------------------------------------------------------------------------
import io
import os
import re
import shutil
import subprocess
import sys

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
EXE = os.path.join(RACINE, "Build", "Bin", "Release-Windows", "NK3DModeler", "NK3DModeler.exe")
ETAT = os.path.join(RACINE, "Applications", "NK3DModeler", "src", "NK3DModeler",
                    "Shell", "NkModelerInput.h")
JOURNAL = os.path.join(RACINE, "logs", "app.log")
PROJET = os.path.expanduser("~/NK3DModeler/AgentTest").replace("\\", "/")
COPIE = os.path.join(os.path.dirname(__file__), "_projet_sauvegarde")
FICHIER = "D:/Rodolf/manequin/XBot/XBot.fbx"


def ilyana():
    out = subprocess.run(["tasklist"], capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if "NKIlyana" in l]


def projet_sauver():
    if os.path.isdir(COPIE):
        shutil.rmtree(COPIE)
    shutil.copytree(PROJET, COPIE)


def projet_restituer():
    if os.path.isdir(COPIE):
        shutil.rmtree(PROJET)
        shutil.copytree(COPIE, PROJET)
        shutil.rmtree(COPIE)


def cartes_du_fichier():
    """Le nombre de cartes ECRITES dans le .nk3dm -- la source de verite du
    projet au repos, pas ce que l'application en memoire croit avoir."""
    p = os.path.join(PROJET, "AgentTest.nk3dm")
    return io.open(p, encoding="utf-8", errors="replace").read().count('"nature"')


def importer():
    env = dict(os.environ)
    env["NK_OPEN_RECENT"] = "0"
    env["NK_IMPORT_FILE"] = FICHIER
    env["NK_AGENT_EXIT"] = "120"
    subprocess.run([EXE], cwd=RACINE, env=env, capture_output=True, timeout=240)
    txt = io.open(JOURNAL, encoding="utf-8", errors="replace").read()
    return re.findall(r"MESURE creation : model .*? carte=(-?\d+) fichier=(\S+)", txt)


def construire():
    r = subprocess.run("jenga build --target NK3DModeler --config Release",
                       cwd=RACINE, capture_output=True, timeout=900, shell=True)
    s = (r.stdout or b"").decode("utf-8", "replace") + (r.stderr or b"").decode("utf-8", "replace")
    return "SUCCESS" in s


def mutation_appliquer():
    src = io.open(ETAT, encoding="utf-8").read()
    io.open(ETAT + ".avant-mutation", "w", encoding="utf-8", newline="").write(src)
    cible = "\t\t\t\t\tcards.PushBack(NkBrowserCard());"
    assert cible in src, "point de mutation introuvable"
    io.open(ETAT, "w", encoding="utf-8", newline="").write(
        src.replace(cible,
                    "\t\t\t\t\tif (BrowserCount() >= 32) // MUTATION DU TEMOIN : le plafond revient\n"
                    "\t\t\t\t\t\treturn -1;\n" + cible))


def mutation_restaurer():
    shutil.copyfile(ETAT + ".avant-mutation", ETAT)
    os.remove(ETAT + ".avant-mutation")


def passe(nom):
    avant = cartes_du_fichier()
    res = importer()
    cartes = [int(c) for c, _ in res]
    fichiers = [f for _, f in res]
    ecrits = [f for f in fichiers if f != "(non" and f.endswith(".nkmesh")]
    sur_disque = [f for f in ecrits if os.path.isfile(os.path.join(PROJET, f))]
    print("   %-10s cartes du .nk3dm avant : %d" % (nom, avant))
    print("   %-10s cartes rendues par l'import : %s" % (nom, cartes or "AUCUNE"))
    print("   %-10s .nkmesh presents sur le disque : %d/%d" % (nom, len(sur_disque), len(res)))
    ok = bool(res) and all(c >= 32 for c in cartes) and len(sur_disque) == len(res)
    return ok, cartes


def main():
    mute = "--mutation" in sys.argv
    print("Ilyana avant :", ilyana() or "ABSENTE")
    projet_sauver()
    rouge = None
    try:
        print("\n-- import dans un projet DEJA a 32 cartes --")
        ok1, c1 = passe("passe 1")
        print("\n-- second import : le projet depasse alors 32 --")
        ok2, c2 = passe("passe 2")
        # Les quatre cartes doivent etre DISTINCTES : un plafond qui recycle le
        # meme indice donnerait 32, 33, 32, 33 et ecraserait les premieres.
        distinctes = len(set(c1 + c2)) == len(c1 + c2)
        print("\n   indices tous distincts : %s (%s)" % (distinctes, c1 + c2))
        verdict = ok1 and ok2 and distinctes
        print("\nVERDICT : %s" % ("VERT -- plus de plafond" if verdict else "ROUGE -- des cartes se perdent"))

        if mute:
            print("\n== MUTATION : on remet un plafond de 32 dans CardAdd ==")
            projet_restituer()
            projet_sauver()
            mutation_appliquer()
            try:
                if not construire():
                    print("build mute : ECHEC")
                    return 2
                okm, cm = passe("mute")
                rouge = not okm
                print("   MUTATION : %s"
                      % ("ROUGE comme attendu" if rouge else "VERTE -- LE TEMOIN NE MESURE RIEN"))
            finally:
                mutation_restaurer()
                construire()
                print("   fichier restaure depuis la copie, binaire reconstruit")
    finally:
        projet_restituer()
        print("   projet de Rodolf restitue depuis la copie")
    print("\nIlyana apres :", ilyana() or "ABSENTE")
    if mute:
        return 0 if (verdict and rouge) else 1
    return 0 if verdict else 1


if __name__ == "__main__":
    sys.exit(main())
