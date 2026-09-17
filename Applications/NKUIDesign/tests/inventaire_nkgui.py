# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# INSTRUMENT VERSIONNE. Croise les TROIS autorites du format `.nkgui` et publie le tableau :
#
#   DIT     -- le validateur de NKUIDesign (NkGuiValidate.h), sa table `kTable` de roles
#   MONTE   -- le monteur de NKGui (NkGuiMonteur.h) : le role a-t-il un `case` dans le
#              dispatch ? (la seule exception documentee est `Callback`, qui a un `case`
#              et pose `aDessine = false` : un point d'entree, pas une chose qui se voit)
#   POSABLE -- l'editeur : la table des roles francais de `NkGuiEcrire.h`, celle que le menu
#              « Role » ECRIT reellement dans le document
#
# Chaque colonne cite le FICHIER et la LIGNE : un « oui » sans ligne vaut « non ».
#
# ⚠️ DEUX PIEGES PAYES EN ECRIVANT CET INSTRUMENT, et ils sont la raison de ces commentaires :
#   1. `NkGuiValidate.h` contient DEUX tables nommees `kTable` -- celle des roles (40) et
#      celle des EFFETS (`fill`, `stroke`, `shadow`, `blur`). La premiere version en comptait
#      44 et publiait « blur » comme un role d'interface.
#   2. Deduire « ne monte rien » en cherchant `aDessine = false` dans le corps d'un `case`
#      est FAUX : `Checkbox` et `Chart` posent ce drapeau dans une BRANCHE (pas d'etat lie),
#      tout en montant dans l'autre. On ne juge donc PAS le corps : on constate le `case`,
#      et on nomme la seule exception que le fichier documente lui-meme.
#
# Usage : python inventaire_nkgui.py [racine]   (defaut : deduite de l'emplacement du script)

import os
import re
import sys

RACINE = sys.argv[1] if len(sys.argv) > 1 else os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))

VALID = os.path.join(RACINE, "Applications", "NKUIDesign", "src", "NKUIDesign", "NkGuiValidate.h")
MONT = os.path.join(RACINE, "Kernel", "Runtime", "NKGui", "src", "NKGui", "Doc", "NkGuiMonteur.h")
ECRIRE = os.path.join(RACINE, "Applications", "NKUIDesign", "src", "NKUIDesign", "NkGuiEcrire.h")
BASE = os.path.join(RACINE, "Applications", "NKUIDesign", "src", "NKUIDesign", "ComposantsBase.h")
PANELS = os.path.join(RACINE, "Applications", "NKUIDesign", "src", "NKUIDesign", "Panels.h")

# Le seul role que le monteur refuse de peindre, et il le dit en toutes lettres.
NE_PEINT_RIEN = {"Callback"}


def lignes(chemin):
    with open(chemin, encoding="utf-8", errors="replace") as f:
        return f.read().split("\n")


def roles_valides():
    """La PREMIERE table `kTable` du validateur : {role: (ligne, nb de proprietes)}."""
    L = lignes(VALID)
    roles, alias, dans_table, dans_alias, table_finie = {}, {}, False, False, False
    for i, l in enumerate(L, 1):
        if "kTable[] = {" in l and not table_finie:
            dans_table = True
            continue
        if "kAliases[] = {" in l:
            dans_alias = True
            continue
        if dans_table:
            m = re.match(r'\s*\{"([A-Za-z]+)",\s*p\w+,\s*(\d+)\}', l)
            if m:
                roles[m.group(1)] = (i, int(m.group(2)))
            elif "};" in l:
                dans_table = False
                table_finie = True
        if dans_alias:
            m = re.match(r'\s*\{"([A-Za-z0-9]+)",\s*"([A-Za-z]+)"\}', l)
            if m:
                alias[m.group(1)] = (m.group(2), i)
            elif "};" in l:
                dans_alias = False
    return roles, alias


def roles_montes():
    """L'enum du monteur, et les `case NkGuiRole::X` de son dispatch."""
    L = lignes(MONT)
    enum, cas, dans_enum = {}, {}, False
    for i, l in enumerate(L, 1):
        if "enum class NkGuiRole" in l:
            dans_enum = True
            continue
        if dans_enum:
            m = re.match(r"\s*([A-Z][A-Za-z]*),?\s*$", l)
            if m:
                enum[m.group(1)] = i
            if "};" in l:
                dans_enum = False
        m = re.match(r"\s*case NkGuiRole::([A-Za-z]+):", l)
        if m:
            cas.setdefault(m.group(1), i)
    return enum, cas


def roles_editeur():
    """Les roles `.nkgui` que l'editeur sait ECRIRE (table des libelles francais)."""
    L = lignes(ECRIRE)
    out, dans = {}, False
    for i, l in enumerate(L, 1):
        if "NkRoleFr k[] = {" in l:
            dans = True
            continue
        if dans:
            m = re.search(r'",\s*"([A-Za-z]+)"\}', l)
            if m:
                out.setdefault(m.group(1), i)
            if re.match(r"\s*\};", l):
                dans = False
    return out


def composants_deposables():
    """Ce que la Palette et la Bibliotheque savent DEPOSER (le registre, a l'execution)."""
    base = []
    for i, l in enumerate(lignes(BASE), 1):
        # un composant se reconnait a sa FAMILLE : c'est ce qui le distingue d'un parametre
        # (`{"valeur", "Valeur", NkParamKind::...}`) ou d'une variante.
        m = re.match(r'\s*\{"([a-z_]+)",\s*"[^"]*",\s*NkFamille::', l)
        if m:
            base.append((m.group(1), i))
    kit = []
    for i, l in enumerate(lignes(PANELS), 1):
        m = re.search(r"NkComponentRegistry::Register\((Nk\w+Decl)\(\)\)", l)
        if m:
            kit.append((m.group(1), i))
    return base, kit


def main():
    valides, alias = roles_valides()
    _enum, cas = roles_montes()
    ecrits = roles_editeur()
    base, kit = composants_deposables()

    print("=== INVENTAIRE .nkgui : DIT / MONTE / POSABLE ===")
    print("racine : %s" % RACINE)
    print()
    print("%-14s %-22s %-26s %s" % ("ROLE", "DIT (validateur)", "MONTE (monteur)", "POSABLE (editeur)"))
    print("-" * 92)
    n_monte = n_ecrit = 0
    for r in sorted(valides):
        ln, nprops = valides[r]
        dit = "oui  L%-5d p=%d" % (ln, nprops)
        if r in cas and r not in NE_PEINT_RIEN:
            monte = "oui  L%d" % cas[r]
            n_monte += 1
        elif r in cas:
            monte = "NON  L%d (point d'entree)" % cas[r]
        else:
            par_alias = [a for a, (cible, _) in alias.items() if cible == r and a in cas]
            monte = ("oui par l'alias %s" % par_alias[0]) if par_alias else "non"
            if par_alias:
                n_monte += 1
        pose = ("oui  L%d" % ecrits[r]) if r in ecrits else "non"
        if r in ecrits:
            n_ecrit += 1
        print("%-14s %-22s %-26s %s" % (r, dit, monte, pose))
    print("-" * 92)
    n = len(valides)
    print("roles DITS par le format      : %d" % n)
    print("roles MONTES par le monteur   : %d   (soit %d qui ne montent pas)" % (n_monte, n - n_monte))
    print("roles que l'editeur sait DIRE : %d   (soit %d qu'il ne sait pas ecrire)" % (n_ecrit, n - n_ecrit))
    print()
    print("roles du MONTEUR absents du vocabulaire du validateur (deux autorites qui divergent) :")
    inconnus = [k for k in cas if k not in valides and k != "Inconnu"]
    for k in sorted(inconnus):
        marque = "  -- et il ne peint rien : c'est un point d'entree" if k in NE_PEINT_RIEN else ""
        print("   %-12s case L%d%s" % (k, cas[k], marque))
    if not inconnus:
        print("   (aucun)")
    print()
    print("COMPOSANTS DEPOSABLES (ce que la Palette et la Bibliotheque offrent) : %d" % (len(base) + len(kit)))
    print("   de base (ComposantsBase.h) : " + ", ".join(nom for nom, _ in base))
    print("   du kit   (Panels.h)        : " + ", ".join("%s L%d" % (nom, l) for nom, l in kit))


if __name__ == "__main__":
    main()
