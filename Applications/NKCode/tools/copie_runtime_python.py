#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

LE RUNTIME PYTHON, POSE A COTE DE `NKCode.exe` — l'etape de construction qui
n'avait jamais ete ecrite.
=============================================================================

LE DEFAUT, MESURE (25/09). `NKCode.exe` construit sortait en **127** : le
chargeur de Windows ne trouvait pas `python312.dll`. Ce n'etait ni une DLL
absente de la machine ni un defaut de code :

  - NKCode se lie a `python312` EN DUR (`_PYEMBED_LIBNAME`, `links()` dans
    `NKCode.jenga`) : la DLL doit donc etre A COTE DE L'EXECUTABLE au
    chargement, pas quelque part dans un sous-dossier `tools/` ;
  - la machine de Rodolf porte Python **3.13** et **3.14**, jamais 3.12 : la
    seule `python312.dll` du monde connu est celle que le depot vendorise dans
    `Externals/Libs/PythonEmbed/runtime/` ;
  - et **rien ne la copiait**. `Build/Bin/Release-Windows/NKCode/` ne contenait
    que `NKCode.exe`. Le commentaire de `NKCode.jenga` l'annoncait lui-meme :
    « voir Phase 3, NkEmbeddedJenga, pour l'etape de copie postbuild une fois le
    code reel en place ». Le code reel est en place depuis longtemps ;
    l'etape, non.

⚠️ CE QUE CE SCRIPT NE FAIT PAS, ET C'EST DELIBERE :
  - **il n'ecrit rien dans `dist/`** : `dist/` est un produit de
    `MakeNkCodeDist.py`, pas un cache de construction ;
  - **il ne copie que ce qui manque ou a change** (taille + date a la seconde
    pres) : une construction qui ne touche pas au runtime ne recopie pas 18 Mo ;
  - **il n'ajoute rien au depot** : la source est `Externals/Libs/PythonEmbed`,
    deja versionnee ; la destination est sous `Build/`, deja ignoree.

⚠️ ET IL NE PREND PAS L'ARBITRAGE. Faut-il un jour suivre le Python DE LA
   MACHINE (3.13 chez Rodolf) au lieu du CPython vendorise ? C'est une decision
   de Rodolf, pas la mienne, et elle est posee dans le rapport. Ce script tient
   la situation ACTUELLE : un CPython embarque, fige, qui ne depend de rien
   d'installe.

Usage :  copie_runtime_python.py <racine du depot> <dossier de NKCode.exe>
Sortie :  une ligne par etat, et un code de sortie non nul si le runtime est
          introuvable -- un echec muet ici redonnerait le 127, plus tard et
          sans explication.
"""

import io
import os
import shutil
import sys

# Ce qui doit etre A COTE de l'executable pour que le CHARGEUR trouve son compte.
# Le reste du runtime (les `.pyd`, les `.dll` satellites) est charge PLUS TARD,
# par Python lui-meme, et suit donc le meme dossier.
# ⚠️ LA DLL DE PYTHON N'EST PAS DANS CETTE LISTE, et c'est voulu : son nom est
#    DEDUIT de `_PYEMBED_LIBNAME` plus bas. Un nom recopie ici se perimerait au
#    premier changement de version, et le controle final verifierait alors une
#    DLL que plus personne n'attend -- un vert sur la mauvaise question.
INDISPENSABLES_FIXES = ("python3.dll", "vcruntime140.dll")


def aJour(src: str, dst: str) -> bool:
    """Meme taille ET meme date (a la seconde). Deux fichiers qui different par
    l'un des deux sont recopies : on ne compare pas les octets, ce serait lire
    18 Mo a chaque construction pour un gain nul."""
    if not os.path.exists(dst):
        return False
    a, b = os.stat(src), os.stat(dst)
    return a.st_size == b.st_size and int(a.st_mtime) == int(b.st_mtime)


def main() -> int:
    if len(sys.argv) < 3:
        print("[nkcode-runtime] REFUS : usage <racine> <dossier de sortie>")
        return 2
    racine, sortie = sys.argv[1], sys.argv[2]
    source = os.path.join(racine, "Externals", "Libs", "PythonEmbed", "runtime")

    if not os.path.isdir(source):
        # REFUS NOMME. Sans lui, la construction reussirait et le binaire
        # sortirait en 127 a l'execution -- c'est-a-dire exactement le defaut
        # qu'on repare, decale dans le temps.
        print("[nkcode-runtime] REFUS : runtime Python introuvable dans " + source)
        print("[nkcode-runtime]         NKCode se lie a python312 EN DUR ; sans ce dossier,")
        print("[nkcode-runtime]         le binaire construit ne demarrera pas (code 127).")
        return 1

    # ── LE CRITERE DE DERIVE DE VERSION ─────────────────────────────
    # Deux verites sur la meme chose : `_PYEMBED_LIBNAME` dans `NKCode.jenga` dit a
    # quoi le binaire SE LIE, et le contenu de `PythonEmbed/runtime` dit ce qu'on
    # COPIE. Le jour ou l'un bouge sans l'autre -- une montee a 3.13 du vendor, ou
    # un changement de `links()` -- la construction reussirait et le binaire
    # mourrait au demarrage sur 0xC0000135. C'est exactement le defaut qu'on
    # repare, et il reviendrait a la prochaine version de Python.
    # On CONFRONTE donc les deux sources, au lieu de croire l'une des deux.
    jenga = os.path.join(racine, "Applications", "NKCode", "NKCode.jenga")
    libname = ""
    if os.path.isfile(jenga):
        for ligne in io.open(jenga, encoding="utf-8", errors="replace"):
            if ligne.strip().startswith("_PYEMBED_LIBNAME"):
                bout = ligne.split("=", 1)[1].strip()
                libname = bout.strip().strip('"').strip("'").split("#")[0].strip().strip('"')
                break
    if libname:
        attendue = libname + ".dll"
        if not os.path.isfile(os.path.join(source, attendue)):
            presentes = [n for n in os.listdir(source)
                         if n.startswith("python") and n.endswith(".dll")]
            print("[nkcode-runtime] REFUS : NKCode.jenga se lie a « %s » mais le runtime"
                  % libname)
            print("[nkcode-runtime]         vendorise ne porte pas %s (il porte : %s)."
                  % (attendue, ", ".join(presentes) if presentes else "aucune python*.dll"))
            print("[nkcode-runtime]         Les deux doivent bouger ENSEMBLE, sinon le binaire")
            print("[nkcode-runtime]         construit meurt au demarrage sur 0xC0000135.")
            return 1

    if not os.path.isdir(sortie):
        os.makedirs(sortie, exist_ok=True)

    copies, ajour, octets = 0, 0, 0
    for nom in sorted(os.listdir(source)):
        s = os.path.join(source, nom)
        if not os.path.isfile(s):
            continue
        d = os.path.join(sortie, nom)
        if aJour(s, d):
            ajour += 1
            continue
        shutil.copy2(s, d)  # copy2 preserve la date : c'est ce qui rend `aJour` vrai ensuite
        copies += 1
        octets += os.path.getsize(s)

    # LE CONTROLE QUI COMPTE, et il porte sur la DESTINATION, pas sur la source :
    # ce qu'on verifie, c'est que le binaire trouvera son runtime -- pas qu'on a
    # cru le copier.
    attendues = list(INDISPENSABLES_FIXES)
    if libname:
        attendues.append(libname + ".dll")
    manquants = [n for n in attendues if not os.path.isfile(os.path.join(sortie, n))]
    if manquants:
        print("[nkcode-runtime] REFUS : apres copie, il manque encore " + ", ".join(manquants))
        return 1

    print("[nkcode-runtime] %d fichier(s) copie(s) (%.1f Mo), %d deja a jour -> %s"
          % (copies, octets / 1048576.0, ajour, sortie))
    return 0


if __name__ == "__main__":
    sys.exit(main())
