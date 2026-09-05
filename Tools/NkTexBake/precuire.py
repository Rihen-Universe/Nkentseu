#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
"""Pré-cuisson des textures d'une distribution — l'étape Jenga.

À quoi ça sert
--------------
La cuisson paresseuse (au premier chargement) suffit pendant le développement :
personne n'a rien à faire, et le second lancement est déjà rapide. Mais pour une
**distribution**, le premier lancement est celui du joueur — c'est justement
celui qu'on ne veut pas payer. Ce script cuit tout d'avance et livre le cache.

Ce n'est PAS une seconde implémentation du four : il appelle `NkTexBake`, qui
calcule l'empreinte par la même fonction que le moteur (`NkTexCacheNommage`).
Deux copies de ce calcul divergeraient sans qu'aucune erreur ne sorte.

Usage
-----
    python Tools/NkTexBake/precuire.py [dossiers...] [--config Release] [--forcer]

Sans dossier, il prend `Resources/`. Le cache atterrit dans `Build/Cache/Assets/`
(ignoré par git : un actif cuit est un dérivé, comme un `.obj`).

Le brancher dans Jenga
----------------------
Dans le `.jenga` de l'application à distribuer, après la construction :

    postbuildcommands(['python Tools/NkTexBake/precuire.py Resources'])

Volontairement NON branché aujourd'hui : cuire tout `Resources/` allonge chaque
construction de plusieurs secondes pour un bénéfice qui n'existe qu'au moment de
distribuer. C'est un geste de distribution, pas un geste de compilation.
"""
import os
import subprocess
import sys


def trouver_four(config):
    """Le binaire du four, dans la configuration demandée."""
    candidats = [
        os.path.join("Build", "Bin", "%s-Windows" % config, "NkTexBake", "NkTexBake.exe"),
        os.path.join("Build", "Bin", "%s-Linux" % config, "NkTexBake", "NkTexBake"),
        os.path.join("Build", "Bin", "%s-macOS" % config, "NkTexBake", "NkTexBake"),
    ]
    for c in candidats:
        if os.path.isfile(c):
            return c
    return None


def main(argv):
    config = "Release"
    forcer = False
    dossiers = []

    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--config" and i + 1 < len(argv):
            i += 1
            config = argv[i]
        elif a == "--forcer":
            forcer = True
        elif a in ("-h", "--help"):
            print(__doc__)
            return 0
        else:
            dossiers.append(a)
        i += 1

    if not dossiers:
        dossiers = ["Resources"]

    four = trouver_four(config)
    if four is None:
        print("[precuire] le four n'est pas construit : `jenga build --target NkTexBake --config %s`" % config)
        return 1

    code = 0
    for d in dossiers:
        if not os.path.isdir(d):
            print("[precuire] dossier absent, ignore : %s" % d)
            continue
        cmd = [four, "--dossier", d]
        if forcer:
            cmd.append("--forcer")
        print("[precuire] %s" % " ".join(cmd))
        r = subprocess.run(cmd)
        # Un refus (un `.hdr` flottant, par exemple) n'est pas une panne : le four
        # le dit ligne par ligne et rend 0. Seul un plantage remonte ici.
        if r.returncode != 0:
            code = r.returncode
    return code


if __name__ == "__main__":
    sys.exit(main(sys.argv))
