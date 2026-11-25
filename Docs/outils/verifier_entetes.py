# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# -*- coding: utf-8 -*-
"""
Vérifie une écriture de documentation DANS des en-têtes (décision de Rodolf, 25/11/2026 : les descriptions
manquantes s'écrivent dans le code, et cela se vérifie).

  1. RIEN que des commentaires : les jetons hors commentaires de chaque en-tête sont identiques avant et après
     (jetons maison : identifiants, nombres, chaînes, ponctuation ; les blancs et les commentaires ne comptent pas ;
     toutes les branches #if sont comparées, pas seulement celle de la machine) ;
  2. chaque phrase TIRÉE DU CODE est vraie, relue contre la définition qu'elle décrit :
       « Garde son argument quand ... »  : la macro vaut __VA_ARGS__ sous #ifdef X, et rien sous #else ;
       « Retire son argument quand ... » : l'inverse ;
       « Définie, sans valeur, quand ... » : toutes ses définitions sont vides ;
       « Vaut 1 quand ... »               : toutes ses définitions valent 1.

--contre-epreuve : (a) un jeton changé dans une copie en mémoire doit faire échouer 1 ; (b) « Garde » échangé
contre « Retire » dans une description doit faire échouer 2.

Usage : py -3 -I Docs/outils/verifier_entetes.py --avant <dossier des en-têtes d'avant> --apres <dossier> [--contre-epreuve]
"""
import argparse
import re
import sys
from pathlib import Path

_JETON = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|[A-Za-z_]\w*|\d[\w.]*|##|::|->|<<|>>|[<>=!&|+\-*/%^]=|&&|\|\||\+\+|--|\.\.\.|\S')


def sans_commentaires(t: str) -> str:
    out, i, n = [], 0, len(t)
    while i < n:
        if t.startswith("//", i):
            j = t.find("\n", i)
            i = n if j < 0 else j
        elif t.startswith("/*", i):
            j = t.find("*/", i + 2)
            out.append(" ")
            i = n if j < 0 else j + 2
        elif t[i] in "\"'":
            q, j = t[i], i + 1
            while j < n and t[j] != q and t[j] != "\n":
                j += 2 if t[j] == "\\" else 1
            out.append(t[i:j + 1])
            i = j + 1
        else:
            out.append(t[i])
            i += 1
    return "".join(out)


def jetons(t: str):
    # une ligne continuée par « \ » est une seule ligne logique : la barre ne compte pas comme jeton
    return _JETON.findall(sans_commentaires(t).replace("\\\r\n", " ").replace("\\\n", " "))


def definitions(texte: str, nom: str):
    out, pile = [], []
    for l in texte.replace("\r\n", "\n").split("\n"):
        s = l.strip()
        if re.match(r"#\s*(if|ifdef|ifndef)\b", s):
            pile.append(s)
        elif re.match(r"#\s*(elif|else)\b", s) and pile:
            pile[-1] = s
        elif re.match(r"#\s*endif\b", s) and pile:
            pile.pop()
        m = re.match(r"\s*#\s*define\s+" + re.escape(nom) + r"\b(.*)$", l)
        if m:
            out.append((pile[-1] if pile else "", re.sub(r"//.*$", "", m.group(1)).strip()))
    return out


def phrases_fausses(texte: str):
    fautes = []
    for m in re.finditer(r"/\*\* @brief (Garde son argument|Retire son argument|Définie, sans valeur|Vaut 1) quand ([^*]*)\*/\s*\n\s*#\s*define\s+(\w+)", texte):
        genre, nom = m.group(1), m.group(3)
        d = definitions(texte, nom)
        ok = False
        if genre in ("Garde son argument", "Retire son argument") and len(d) == 2:
            (c1, v1), (c2, v2) = d
            if c1.startswith("#ifdef") and c2.startswith("#else"):
                ok = (v1, v2) == (("(...) __VA_ARGS__", "(...)") if genre.startswith("Garde") else ("(...)", "(...) __VA_ARGS__"))
        elif genre == "Définie, sans valeur":
            ok = bool(d) and all(v == "" for _, v in d)
        elif genre == "Vaut 1":
            ok = bool(d) and all(v == "1" for _, v in d)
        if not ok:
            fautes.append(f"{nom} : « {genre} » ne décrit pas ses définitions {d[:3]}")
    return fautes


def verifier(avant: dict, apres: dict):
    fautes = []
    for nom, t in apres.items():
        if nom not in avant:
            fautes.append(f"[jetons] {nom} : absent d'avant")
            continue
        ja, jb = jetons(avant[nom]), jetons(t)
        if ja != jb:
            i = next((k for k in range(min(len(ja), len(jb))) if ja[k] != jb[k]), min(len(ja), len(jb)))
            fautes.append(f"[jetons] {nom} : différent au jeton {i} ({ja[i:i + 3]} contre {jb[i:i + 3]})")
        for x in phrases_fausses(t):
            fautes.append(f"[phrase] {nom} : {x}")
    return fautes


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser()
    ap.add_argument("--avant", required=True)
    ap.add_argument("--apres", required=True)
    ap.add_argument("--contre-epreuve", action="store_true")
    a = ap.parse_args()
    avant = {p.name: p.read_bytes().decode("utf-8") for p in Path(a.avant).glob("*.h")}
    apres = {p.name: p.read_bytes().decode("utf-8") for p in Path(a.apres).glob("*.h")}
    fautes = verifier(avant, apres)
    nb = sum(len(re.findall(r"/\*\* @brief (?:Garde son argument|Retire son argument|Définie, sans valeur|Vaut 1) quand", t)) for t in apres.values())
    ajout = sum(t.count("\n") - avant.get(k, "").count("\n") for k, t in apres.items())
    for f in fautes:
        print("ROUGE " + f)
    print(f"verifier_entetes : {'VERT' if not fautes else 'ROUGE'} ({len(apres)} en-têtes ; jetons hors commentaires "
          f"comparés ; {nb} phrases tirées du code relues ; {ajout} lignes ajoutées)")
    if not a.contre_epreuve:
        return 1 if fautes else 0
    ok = True
    nom = next(k for k, t in apres.items() if re.search(r"#\s*define\s+\w+\s+1\b", t))
    mut = dict(apres)
    mut[nom] = re.sub(r"(#\s*define\s+\w+\s+)1\b", r"\g<1>2", mut[nom], count=1)
    fa = [x for x in verifier(avant, mut) if x.startswith("[jetons]")]
    print(f"contre-épreuve a (un jeton changé dans {nom}) : {'ROUGE, attendu' if fa else 'VERTE : RATÉE'}")
    ok &= bool(fa)
    nom = next((k for k, t in apres.items() if "@brief Garde son argument" in t), None)
    if nom:
        mut = dict(apres)
        mut[nom] = mut[nom].replace("@brief Garde son argument", "@brief Retire son argument", 1)
        fb = [x for x in verifier(apres, mut) if x.startswith("[phrase]")]
        print(f"contre-épreuve b (« Garde » devient « Retire » dans {nom}) : {'ROUGE, attendu' if fb else 'VERTE : RATÉE'}")
        ok &= bool(fb)
    print("contre-épreuves : " + ("toutes rouges, comme attendu" if ok else "AU MOINS UNE RATÉE"))
    return 1 if (fautes or not ok) else 0


if __name__ == "__main__":
    sys.exit(main())
