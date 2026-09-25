#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
"""Un kit se lie-t-il avec une AUTRE chaine d'outils que celle qui l'a produit ?

Pourquoi cet instrument existe (25/09/2026) : le kit NKWindow etait produit par
le clang de MSYS2 (libstdc++, mingw-w64 recent) et consomme par le clang que
NKCode embarque (llvm-mingw 20240619 : libc++, mingw-w64 plus ancien). Deux
symboles ne traversaient pas — `stat64i32` et
`std::out_of_range::out_of_range(char const*)` — et rien ne le disait avant que
l'utilisateur ne voie « ld.lld: error: undefined symbol ». Un kit qui se lie
chez son auteur ne prouve RIEN sur la machine de celui qui le recoit.

Ce que le script fait : pour chaque archive du kit, il prend les symboles
INDEFINIS, retire ceux que le kit definit lui-meme, retire ceux que la chaine
d'outils cible definit, et imprime ce qui reste. Ce qui reste est exactement ce
qui fera echouer le lien chez le consommateur.

Il rougit (code 1) des qu'il reste un symbole, et nomme le fichier objet.

Usage :
  python Tools/verif_kit_lien_croise.py <dossier-du-kit> <racine-de-la-chaine>
Exemple :
  python Tools/verif_kit_lien_croise.py Build/Kit/nkwindow \
         D:/Projets/2026/Distributions/NKCode/tools/compilers/llvm-mingw
"""

import os
import re
import subprocess
import sys

# Symboles que l'editeur de liens resout sans aucune archive : les fonctions de
# l'API Windows arrivent par les bibliotheques d'importation que TOUTE chaine
# Windows possede, et les intrinseques du compilateur n'ont pas d'archive.
IGNORES = re.compile(
    r"^(__imp_|\.refptr\.|__x86\.|__chkstk|_alloca|__main$)"
)


def trouver_nm(racine_chaine):
    for nom in ("llvm-nm.exe", "llvm-nm", "nm.exe", "nm"):
        chemin = os.path.join(racine_chaine, "bin", nom)
        if os.path.isfile(chemin):
            return chemin
    raise SystemExit("ECHEC : aucun nm sous %s/bin — mesure impossible, "
                     "on ne conclut pas." % racine_chaine)


def symboles(nm, fichier, indefinis):
    """Les symboles d'une archive. Leve si nm echoue : un instrument muet
    ne doit jamais passer pour un resultat vide (piege paye le 25/09)."""
    args = [nm, "-u" if indefinis else "--defined-only", fichier]
    r = subprocess.run(args, capture_output=True, text=True, errors="replace")
    if r.returncode != 0 and not r.stdout.strip():
        raise SystemExit("ECHEC : %s a rendu %d sur %s\n%s"
                         % (os.path.basename(nm), r.returncode, fichier,
                            r.stderr.strip()[:400]))
    out = set()
    for ligne in r.stdout.splitlines():
        m = re.match(r"^\s*(?:[0-9a-fA-F]+)?\s*[A-Za-z?]\s+(\S+)\s*$", ligne)
        if m:
            out.add(m.group(1))
    return out


def archives(dossier, motifs):
    trouves = []
    for base, _, fichiers in os.walk(dossier):
        for f in fichiers:
            if f.lower().endswith(motifs):
                trouves.append(os.path.join(base, f))
    return trouves


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    kit, chaine = sys.argv[1], sys.argv[2]
    if not os.path.isdir(kit):
        raise SystemExit("ECHEC : kit introuvable : %s" % kit)
    if not os.path.isdir(chaine):
        raise SystemExit("ECHEC : chaine introuvable : %s" % chaine)

    nm = trouver_nm(chaine)
    libs_chaine = archives(chaine, (".a",))
    if not libs_chaine:
        raise SystemExit("ECHEC : aucune archive .a sous %s — mesure "
                         "impossible." % chaine)

    print("kit    : %s" % os.path.abspath(kit))
    print("chaine : %s (%d archives)" % (os.path.abspath(chaine),
                                         len(libs_chaine)))

    fournis = set()
    for lib in libs_chaine:
        fournis |= symboles(nm, lib, indefinis=False)
    print("        la chaine definit %d symboles" % len(fournis))

    total_residu = 0
    for cible in sorted(os.listdir(os.path.join(kit, "lib"))):
        rep = os.path.join(kit, "lib", cible)
        if not os.path.isdir(rep):
            continue
        libs_kit = archives(rep, (".lib", ".a"))
        definis_kit = set()
        for lib in libs_kit:
            definis_kit |= symboles(nm, lib, indefinis=False)

        residu = {}
        for lib in libs_kit:
            for s in symboles(nm, lib, indefinis=True):
                if s in definis_kit or s in fournis or IGNORES.match(s):
                    continue
                residu.setdefault(s, set()).add(os.path.basename(lib))

        if residu:
            total_residu += len(residu)
            print("\n  %s : %d symbole(s) que la chaine cible NE RESOUT PAS"
                  % (cible, len(residu)))
            for s in sorted(residu):
                print("    %-60s <- %s" % (s, ", ".join(sorted(residu[s]))))
        else:
            print("\n  %s : aucun symbole residuel (%d archives)"
                  % (cible, len(libs_kit)))

    if total_residu:
        print("\nROUGE : %d symbole(s) ne traversent pas. Le lien echouera "
              "chez le consommateur." % total_residu)
        return 1
    print("\nVERT : tout ce que le kit reclame est fourni par lui-meme ou par "
          "la chaine cible.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
