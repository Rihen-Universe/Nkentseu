#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verifier_la_pile.py — le controle qui manquait.

CE QU'IL EXISTE POUR
--------------------
La pile offerte au stagiaire est decrite CINQ fois, dans cinq fichiers, et rien
ne les tenait ensemble :

  1. NkcLayout.h            kRepo[]              13 racines d'inclusion (mode depot)
  2. Distribuer.ps1         $modules             13 dossiers -> include/<Nom>
  3. NkcModuleCompiler.h    StackLibs kLibs[]    12 bibliotheques a lier
  4. Distribuer.ps1         $libs                12 bibliotheques a copier
  5. ConquerorLab.jenga     nkentseudependson    ce qui est REELLEMENT construit

Le 2026-08-15, `NKSerialization` figurait dans 1, 2, 3 et 4 — et manquait dans
5, la seule qui construit. Consequences MESUREES : `Distribuer.ps1` s'arretait
sur « Bibliotheque introuvable », donc AUCUN kit ne pouvait exister ; et tout
module de stagiaire echouait au lien sur `-lNKSerialization`, y compris
l'exemple livre.

Une divergence entre ces listes ne casse rien au moment ou on l'ecrit. Elle
casse le kit, plus tard, chez quelqu'un d'autre.

CE QU'IL VERIFIE, ET CE QU'IL NE VERIFIE PAS
--------------------------------------------
Il verifie la COHERENCE des cinq listes entre elles. Il ne verifie pas que les
dossiers existent, ni que les bibliotheques se construisent : ca, c'est le role
de `Distribuer.ps1`, qui echoue deja proprement dessus.

Ce cadrage est reimprime a chaque execution, dans la SORTIE et pas seulement
ici : un en-tete se lit une fois, une sortie se lit a chaque fois. Sans lui,
quelqu'un lira « controle vert » comme « la pile est saine ».

USAGE
-----
    python verifier_la_pile.py            depuis n'importe ou
    -> code de sortie 0 si tout concorde, 1 sinon.
"""
import os
import re
import sys

LAB = os.path.dirname(os.path.abspath(__file__))


def lire(rel):
    chemin = os.path.join(LAB, rel)
    with open(chemin, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def bloc(texte, debut, fin):
    """Le texte entre `debut` et le premier `fin` qui suit. Rend None si absent —
    un motif qui ne matche plus est un ECHEC, jamais une liste vide."""
    i = texte.find(debut)
    if i < 0:
        return None
    j = texte.find(fin, i)
    return texte[i:j] if j > 0 else None


# ── 1. NkcLayout.h : les racines d'inclusion en mode depot ───────────────────
def includes_cpp():
    b = bloc(lire("src/ConquerorLab/NkcLayout.h"), "static const char *kRepo[]", "};")
    if b is None:
        return None
    return [m.replace("\\", "/").strip("/") for m in re.findall(r'"([^"]+)"', b)]


# ── 2. Distribuer.ps1 : les dossiers recopies dans include/ ──────────────────
def includes_ps1():
    b = bloc(lire("Distribuer.ps1"), "$modules = [ordered]@{", "}")
    if b is None:
        return None
    # 'Kernel\System\NKLogger\src\NKLogger' = 'NKLogger'
    out = []
    for src, nom in re.findall(r"'([^']+)'\s*=\s*'([^']+)'", b):
        src = src.replace("\\", "/")
        # le .ps1 descend d'un cran (…/src/NKLogger) ; NkcLayout s'arrete a /src
        out.append((src.rsplit("/", 1)[0] if src.endswith("/" + nom) else src, nom))
    return out


# ── 3 et 4. Les bibliotheques, des deux cotes ────────────────────────────────
def libs_cpp():
    b = bloc(lire("src/ConquerorLab/NkcModuleCompiler.h"),
             "static const char *const kLibs[]", "};")
    return None if b is None else re.findall(r'"([^"]+)"', b)


def libs_ps1():
    b = bloc(lire("Distribuer.ps1"), "$libs = @(", ")")
    return None if b is None else re.findall(r"'([^']+)'", b)


# ── 5. ConquerorLab.jenga : ce qui est REELLEMENT construit ──────────────────
def deps_jenga():
    b = bloc(lire("ConquerorLab.jenga"), "nkentseudependson(", "]")
    return None if b is None else re.findall(r'"([^"]+)"', b)


def main():
    print("=" * 74)
    print("VERIFICATION DE LA PILE OFFERTE AU STAGIAIRE")
    print("=" * 74)
    print("PORTEE  compare entre elles les 5 listes qui decrivent la pile.")
    print("        NE verifie PAS que les dossiers existent ni que les")
    print("        bibliotheques se construisent (role de Distribuer.ps1).")
    print()

    sources = {
        "NkcLayout.h kRepo[]": includes_cpp(),
        "Distribuer.ps1 $modules": includes_ps1(),
        "NkcModuleCompiler.h StackLibs": libs_cpp(),
        "Distribuer.ps1 $libs": libs_ps1(),
        "ConquerorLab.jenga nkentseudependson": deps_jenga(),
    }

    # Un motif qui ne matche plus rendrait des listes vides « toutes egales ».
    # C'est la face « reussir pour la mauvaise raison » : on l'arrete ici.
    fatals = [nom for nom, v in sources.items() if not v]
    if fatals:
        for nom in fatals:
            print(f"ECHEC   liste introuvable ou vide : {nom}")
        print("\n        Le fichier a probablement change de forme. Ce controle ne")
        print("        peut RIEN affirmer tant qu'il ne sait pas relire ses sources.")
        return 1

    inc_cpp = sources["NkcLayout.h kRepo[]"]
    inc_ps1 = sources["Distribuer.ps1 $modules"]
    lib_cpp = sources["NkcModuleCompiler.h StackLibs"]
    lib_ps1 = sources["Distribuer.ps1 $libs"]
    jenga = sources["ConquerorLab.jenga nkentseudependson"]

    for nom, v in sources.items():
        print(f"  lu  {len(v):>3} entree(s)   {nom}")
    print()

    ecarts = []

    # (a) les deux listes de racines designent-elles les memes dossiers ?
    a, b = set(inc_cpp), set(p for p, _ in inc_ps1)
    if a != b:
        for x in sorted(a - b):
            ecarts.append(f"racine dans NkcLayout.h mais PAS dans Distribuer.ps1 : {x}")
        for x in sorted(b - a):
            ecarts.append(f"racine dans Distribuer.ps1 mais PAS dans NkcLayout.h : {x}")

    # (b) les deux listes de bibliotheques
    a, b = set(lib_cpp), set(lib_ps1)
    for x in sorted(a - b):
        ecarts.append(f"bibliotheque liee par l'atelier mais PAS copiee dans le kit : {x}")
    for x in sorted(b - a):
        ecarts.append(f"bibliotheque copiee dans le kit mais PAS liee par l'atelier : {x}")

    # (c) L'INVARIANT QUI A CASSE : tout ce qu'on lie doit etre CONSTRUIT.
    for lib in sorted(set(lib_cpp)):
        if lib not in jenga:
            ecarts.append(
                f"{lib} est liee aux modules du stagiaire mais ABSENTE de "
                f"nkentseudependson : elle ne sera pas construite, donc le kit "
                f"ne pourra pas etre assemble (incident du 2026-08-15)")

    # (d) toute racine de module doit avoir sa bibliotheque, et reciproquement
    noms = set(nom for _, nom in inc_ps1) - {"Conqueror"}
    for x in sorted(noms - set(lib_cpp)):
        ecarts.append(f"{x} est offert en -I au stagiaire mais aucune -l ne l'accompagne")
    for x in sorted(set(lib_cpp) - noms):
        ecarts.append(f"{x} est liee mais ses en-tetes ne sont pas copiees dans le kit")

    if ecarts:
        print(f"ECHEC   {len(ecarts)} divergence(s) :\n")
        for e in ecarts:
            print(f"  - {e}")
        print("\n        Corriger AVANT de fabriquer un kit : ces listes doivent")
        print("        rester d'accord, et rien d'autre ne les y oblige.")
        return 1

    print("OK      les 5 listes concordent.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
