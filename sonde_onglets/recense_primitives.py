#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""recense_primitives.py — (j1) COMBIEN D'AUTRES ?

AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

Canal `onglets.questions.md`, (j1). J'ai trouve DEUX primitives que
`NkModelerComponentPaint` n'implementait pas (`Line`, `Ellipse`), et je ne l'ai
su que par une image. La question : la liste s'arrete-t-elle a deux ?

⚠️ POURQUOI UN SCRIPT ET PAS UNE LECTURE. C'est une lecture a l'oeil qui a laisse
   passer les deux premieres.

⚠️ ET POURQUOI CE SCRIPT A DEJA MENTI UNE FOIS. Ma premiere version cherchait
   les surcharges avec `\\b(\\w+)\\s*\\([^;{]*\\)\\s*(?:const\\s*)?override` : la
   classe `[^;{]*` est GLOUTONNE et traverse les sauts de ligne, si bien qu'elle
   avalait plusieurs declarations et rattachait le mot `override` au mauvais
   nom. Resultat : elle rendait « NON » pour presque tout -- y compris pour
   `Line` et `Ellipse` que je venais d'ecrire. **Et mon temoin negatif est passe
   au vert malgre tout**, puisqu'il demandait seulement que ces deux-la
   apparaissent comme manquantes AVANT le correctif : un script qui repond
   « manquant » a tout satisfait ce test sans rien mesurer. C'est « un negatif
   incapable de refuter », en plein.

   D'ou les DEUX gardes ci-dessous, et elles ne se negocient pas :
     * garde POSITIVE : des primitives dont on SAIT qu'elles sont surchargees
       par chaque peintre doivent ressortir « oui » ;
     * garde NEGATIVE : `Line` et `Ellipse` doivent ressortir manquantes dans la
       version d'AVANT le correctif, et presentes dans celle d'APRES.
   Si l'une des deux tombe, le script s'arrete et ne publie AUCUN tableau.

CE QU'IL DISTINGUE, ET C'EST TOUTE LA QUESTION
  * PURE          : `= 0`. Le compilateur EXIGE une implementation ; un hote qui
                    l'oublie NE COMPILE PAS. Aucun risque silencieux.
  * DEFAUT DERIVE : le corps par defaut fait le travail avec d'autres primitives
                    (`Outline` s'ecrit avec `FillColor`). Ne pas le surcharger
                    est legitime et ne degrade rien.
  * DEFAUT INERTE : le corps par defaut ne dessine RIEN (`return false`, ou vide).
                    Ne pas le surcharger degrade le rendu SANS QUE RIEN NE LE
                    DISE. C'est la famille qui nous interesse.
"""
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")

KIT = "Engine/NKEditorKit/src/NKEditorKit/Components/NkComponentPaint.h"
PEINTRES = {
    "NkGuiComponentPaint": "Engine/NKEditorKit/src/NKEditorKit/Components/NkGuiComponentPaint.h",
    "NkRecordingPaint": "Engine/NKEditorKit/src/NKEditorKit/Components/NkRecordingPaint.h",
    "NkModelerComponentPaint": "Applications/NK3DModeler/src/NK3DModeler/Shell/NkModelerComponentPaint.h",
}

# Ce qu'on EXIGE de retrouver : des primitives PURES, donc forcement surchargees
# par les trois peintres (sinon rien ne compilerait).
GARDE_POSITIVE = ["Fill", "FillColor", "PushClip", "PopClip", "Icon", "TextWidth"]


def sans_commentaires(t):
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)


def nom_avant_parenthese(t, pos_par_ouvrante):
    """L'identifiant qui precede immediatement `(`."""
    k = pos_par_ouvrante - 1
    while k >= 0 and t[k].isspace():
        k -= 1
    fin = k + 1
    while k >= 0 and (t[k].isalnum() or t[k] == "_"):
        k -= 1
    return t[k + 1 : fin]


def surcharges_texte(t):
    """Les noms de methodes marquees `override`.

    On part du mot `override`, on remonte au `)` qui le precede, on apparie les
    parentheses a REBOURS jusqu'a la `(` correspondante, et on lit le nom juste
    avant. Aucune expression gloutonne : l'appariement est explicite.
    """
    t = sans_commentaires(t)
    noms = set()
    for m in re.finditer(r"\boverride\b", t):
        k = m.start() - 1
        while k >= 0 and t[k].isspace():
            k -= 1
        # sauter un eventuel `const` / `noexcept` entre `)` et `override`
        for mot in ("const", "noexcept"):
            if k >= len(mot) - 1 and t[k - len(mot) + 1 : k + 1] == mot:
                k -= len(mot)
                while k >= 0 and t[k].isspace():
                    k -= 1
        if k < 0 or t[k] != ")":
            continue
        prof = 0
        while k >= 0:
            if t[k] == ")":
                prof += 1
            elif t[k] == "(":
                prof -= 1
                if prof == 0:
                    break
            k -= 1
        if k < 0:
            continue
        n = nom_avant_parenthese(t, k)
        if n:
            noms.add(n)
    return noms


def corps_apres(t, i):
    j = t.find(";", i)
    acc = t.find("{", i)
    if acc == -1 or (j != -1 and j < acc):
        return None
    prof, k = 0, acc
    while k < len(t):
        if t[k] == "{":
            prof += 1
        elif t[k] == "}":
            prof -= 1
            if prof == 0:
                return t[acc : k + 1]
        k += 1
    return None


def classe_defaut(corps):
    if corps is None:
        return "PURE"
    c = sans_commentaires(corps)
    c = re.sub(r"\(void\)\s*\w+\s*;", "", c)
    utile = [l.strip() for l in c.strip("{} \n").split("\n") if l.strip()]
    if not utile:
        return "INERTE"
    if all(re.fullmatch(r"return\s+(false|true)\s*;", l) for l in utile):
        return "INERTE"
    # ⚠️ LE REPLI QUI SE NOMME RESTE UN REPLI. Depuis (j2), un defaut inerte
    #    pose un temoin et appelle `NkPaintRepliInerte(...)` : il ne dessine
    #    toujours rien, il l'annonce. Sans cette regle, le classeur les rangeait
    #    en DERIVE -- c'est-a-dire « sans consequence » -- et mon propre
    #    correctif rendait mon propre recensement faux.
    if any("NkPaintRepliInerte" in l for l in utile):
        return "INERTE"
    return "DERIVE"


def primitives():
    t = open(KIT, encoding="utf-8").read()
    out = []
    for m in re.finditer(r"virtual\s+[^;{()]*?\b(\w+)\s*\(", t):
        nom = m.group(1)
        if nom in ("NkComponentPaint",) or nom.startswith("~"):
            continue
        out.append((nom, classe_defaut(corps_apres(t, m.end()))))
    return out


def version_git(ref, chemin):
    return subprocess.run(["git", "show", f"{ref}:{chemin}"], capture_output=True,
                          text=True, encoding="utf-8", check=True).stdout


def main():
    prims = primitives()
    noms = [n for n, _ in prims]
    peintres = {n: surcharges_texte(open(p, encoding="utf-8").read()) for n, p in PEINTRES.items()}

    # ── GARDE POSITIVE ──────────────────────────────────────────────────────
    echecs = []
    for pn, s in peintres.items():
        for g in GARDE_POSITIVE:
            if g not in s:
                echecs.append(f"{pn} devrait surcharger `{g}` (primitive PURE) et le script ne le voit pas")
    # ── GARDE NEGATIVE ──────────────────────────────────────────────────────
    rel = "Applications/NK3DModeler/src/NK3DModeler/Shell/NkModelerComponentPaint.h"
    av = surcharges_texte(version_git("2ba91d68e~1", rel))
    ap = surcharges_texte(version_git("2ba91d68e", rel))
    for g in ("Line", "Ellipse"):
        if g in av:
            echecs.append(f"`{g}` ne devrait PAS etre surchargee avant 2ba91d68e")
        if g not in ap:
            echecs.append(f"`{g}` devrait etre surchargee apres 2ba91d68e")

    if echecs:
        print("⚠️ CE SCRIPT NE MESURE PAS CE QU'IL CROIT — aucun tableau publie :")
        for e in echecs:
            print(f"   - {e}")
        return 1
    print("Gardes du script : POSITIVE et NEGATIVE passees. Le detecteur sait dire oui ET non.\n")

    larg = max(len(n) for n in noms)
    entete = f"  {'primitive':<{larg}}  {'defaut':<7}  " + "  ".join(f"{n[:22]:<22}" for n in PEINTRES)
    print(entete)
    print("  " + "-" * (len(entete) - 2))
    manquantes = []
    for nom, cls in prims:
        cols = [f"{('oui' if nom in peintres[pn] else '-- NON --'):<22}" for pn in PEINTRES]
        print(f"  {nom:<{larg}}  {cls:<7}  " + "  ".join(cols))
        if cls == "INERTE" and nom not in peintres["NkModelerComponentPaint"]:
            manquantes.append(nom)

    inertes = [n for n, c in prims if c == "INERTE"]
    # ⚠️ GARDE DE STABILITE : le recensement du 14/09 en a compte HUIT. Si ce
    #    nombre change sans qu'on ait touche a l'interface, c'est le CLASSEUR qui
    #    a derive -- ce qui est exactement arrive apres (j2).
    if len(inertes) != 8:
        print(f"  ⚠️ ATTENDU 8 primitives INERTES, le classeur en voit {len(inertes)} : "
              f"{inertes}\n     Soit l'interface a change, soit le classeur derive. "
              f"Verifier AVANT de lire le tableau.")
    print()
    print(f"  a DEFAUT INERTE : {len(inertes)} -> {inertes}")
    print(f"  NON implementees par le modeleur : {manquantes}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
