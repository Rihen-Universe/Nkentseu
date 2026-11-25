# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# -*- coding: utf-8 -*-
"""
Le banc du générateur : ce que le site AFFICHE suit ce que le code et les .jenga DISENT, et les règles « aucun
exemple ne ment » tiennent. Chaque cas reconstruit le site dans un dossier temporaire à partir d'une copie MUTÉE
des données (api.json, graphe.json, exemples.json, pages) et regarde la sortie.

  1. hiérarchie : une classe dérivée ajoutée apparaît sous sa base (texte et diagramme) ; retirée, elle disparaît ;
  2. dépendances : une dépendance ajoutée dans graphe.json apparaît sur la page du module (texte et diagramme) ;
  3. couches : un module de Foundation qui dépendrait de System fait apparaître l'alerte « À REVOIR » ;
  4. jauge : un @brief retiré fait baisser la couverture affichée ;
  5. manuel : [[un nom qui n'existe pas]] est une erreur de construction ;
  6. manuel : un bloc ```cpp sans fichier derrière est une erreur ; « ```cpp fragment » passe ;
  7. version : un exemple qui emploie un nom absent de l'étiquette publiée est une erreur ;
  8. exemples (jenga docs exemples) : un attendu changé rend l'exemple rouge ; une source plus récente que son
     exécutable (construction ratée ou pas relancée) aussi ; l'original est vert.

Usage (depuis la racine Nkentseu) :
  py -3 -I Docs/outils/banc_generateur.py --construction <dossier> [--jenga <arbre Jenga pour jenga docs exemples>]
Code 0 si tous les cas se comportent comme attendu.
"""

import argparse
import copy
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ICI = Path(__file__).resolve().parent
DOCS = ICI.parent
RACINE = DOCS.parent
GEN = ICI / "generer_site.py"
MOD = "NKPlatform"
PAGE_MOD = "fr/nkentseu/0.1/reference/foundation/nkplatform/index.html"
ACCUEIL = "fr/nkentseu/0.1/index.html"


def generer(tmp: Path, cons: dict, docs: Path) -> tuple:
    """Écrit la construction mutée, lance le générateur, rend (code, sortie texte, dossier du site)."""
    c = tmp / "construction"
    if c.exists():
        shutil.rmtree(c)
    for v, cle in (("0.1", "api"), ("dev", "api_main"), ("main", "api_main")):
        (c / "api" / v / MOD).mkdir(parents=True)
        (c / "api" / v / MOD / "api.json").write_text(json.dumps(cons[cle], ensure_ascii=False), encoding="utf-8")
    (c / "graphe.json").write_text(json.dumps(cons["graphe"], ensure_ascii=False), encoding="utf-8")
    (c / "exemples.json").write_text(json.dumps(cons["exemples"], ensure_ascii=False), encoding="utf-8")
    site = tmp / "site"
    r = subprocess.run([sys.executable, "-I", str(GEN), "--construction", str(c), "--sortie", str(site), "--docs", str(docs)],
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    return r.returncode, r.stdout + r.stderr, site


def lire(site: Path, rel: str) -> str:
    p = site / rel
    return p.read_text(encoding="utf-8") if p.exists() else ""


def main() -> int:
    # la console Windows est en cp1252 (et -I ignore PYTHONIOENCODING) : les messages sortent en UTF-8
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser()
    ap.add_argument("--construction", required=True)
    ap.add_argument("--jenga", default=os.environ.get("JENGA_SRC", ""),
                    help="Arbre Jenga qui contient « jenga docs exemples » (défaut : JENGA_SRC, sinon le Jenga installé)")
    a = ap.parse_args()
    src = Path(a.construction)
    base = {
        "api": json.loads((src / "api" / "0.1" / MOD / "api.json").read_text(encoding="utf-8")),
        "api_main": json.loads((src / "api" / "main" / MOD / "api.json").read_text(encoding="utf-8")),
        "graphe": json.loads((src / "graphe.json").read_text(encoding="utf-8")),
        "exemples": json.loads((src / "exemples.json").read_text(encoding="utf-8")),
    }
    resultats = []

    def cas(nom, ok, detail=""):
        resultats.append(ok)
        print(f"{'OK   ' if ok else 'ÉCHEC'} {nom}{(' : ' + detail) if detail else ''}")

    with tempfile.TemporaryDirectory(prefix="banc_gen_") as t:
        tmp = Path(t)
        docs = tmp / "Docs"
        shutil.copytree(DOCS, docs, ignore=shutil.ignore_patterns("__pycache__"))

        # 0. la référence de départ se construit sans erreur
        code, out, site = generer(tmp, base, docs)
        cas("départ : le site se construit sans erreur", code == 0, out.strip().splitlines()[-1] if out.strip() else "")
        page_mod0 = lire(site, PAGE_MOD)
        accueil0 = lire(site, ACCUEIL)

        # 1. hiérarchie
        typ = next(s for s in base["api"]["symboles"] if s["nature"] == "struct" and not s.get("parent"))
        page_type = "fr/nkentseu/0.1/reference/foundation/nkplatform/" + re.sub(r"[^a-z0-9]+", "-", typ["qualifie"].lower()).strip("-") + "/index.html"
        mut = copy.deepcopy(base)
        derivee = dict(copy.deepcopy(typ), nom="DeriveeDEssai", qualifie=typ["espace"] + "::DeriveeDEssai",
                       id=typ["espace"] + "::DeriveeDEssai", ligne=typ["ligne"] + 1,
                       signature="struct DeriveeDEssai : " + typ["nom"],
                       bases=[{"type": typ["qualifie"], "acces": "public", "virtuelle": False}])
        mut["api"]["symboles"].append(derivee)
        code, out, site = generer(tmp, mut, docs)
        p = lire(site, page_type)
        avec = "DeriveeDEssai" in p and re.search(r"<text[^>]*>DeriveeDEssai</text>", p) is not None
        cas("hiérarchie : une classe dérivée ajoutée apparaît sous sa base (texte et diagramme)", code == 0 and avec)
        mut["api"]["symboles"].remove(derivee)
        code, out, site = generer(tmp, mut, docs)
        cas("hiérarchie : retirée, elle disparaît de l'arbre", "DeriveeDEssai" not in lire(site, page_type))

        # 2. dépendances
        mut = copy.deepcopy(base)
        mut["graphe"]["projets"][MOD]["depend_de"] = sorted(set(mut["graphe"]["projets"][MOD]["depend_de"]) | {"NKCore"})
        code, out, site = generer(tmp, mut, docs)
        p = lire(site, PAGE_MOD)
        avant = re.search(r"(Dépend de : )(.*?)\.", page_mod0)
        apres = re.search(r"(Dépend de : )(.*?)\.", p)
        cas("dépendances : une dépendance ajoutée apparaît (texte)",
            avant and "NKCore" not in avant.group(2) and apres and "NKCore" in apres.group(2))
        cas("dépendances : et dans le diagramme (une boîte et une flèche de plus)",
            p.count('class="arete"') == page_mod0.count('class="arete"') + 1)

        # 3. couches
        mut = copy.deepcopy(base)
        mut["graphe"]["projets"]["NKCore"]["depend_de"] = sorted(set(mut["graphe"]["projets"]["NKCore"]["depend_de"]) | {"NKLogger"})
        code, out, site = generer(tmp, mut, docs)
        p = lire(site, ACCUEIL)
        cas("couches : Foundation qui dépend de System déclenche l'alerte « À REVOIR »",
            "À REVOIR" not in accueil0 and "À REVOIR" in p and "Foundation&gt;System" in p)

        # 4. jauge
        mut = copy.deepcopy(base)
        s0 = next(s for s in mut["api"]["symboles"] if (s.get("doc") or {}).get("brief") and s["nature"] != "champ")
        s0["doc"]["brief"] = ""
        code, out, site = generer(tmp, mut, docs)
        n0 = re.search(r"(\d+) / (\d+) \(", page_mod0)
        n1 = re.search(r"(\d+) / (\d+) \(", lire(site, PAGE_MOD))
        cas("jauge : un @brief retiré fait baisser la couverture",
            n0 and n1 and int(n1.group(1)) == int(n0.group(1)) - 1, f"{n0 and n0.group(0)} puis {n1 and n1.group(0)}")

        # 5 et 6. manuel
        page = docs / "site" / "fr" / "nkentseu" / "manuel" / "foundation" / "nkplatform.md"
        texte = page.read_text(encoding="utf-8")
        page.write_text(texte + "\n\nVoir [[NkFonctionQuiNExistePas]].\n", encoding="utf-8")
        code, out, site = generer(tmp, base, docs)
        cas("manuel : un nom inexistant est une erreur de construction", code != 0 and "NkFonctionQuiNExistePas" in out)
        page.write_text(texte + "\n\n```cpp\nint x = 0;\n```\n", encoding="utf-8")
        code, out, site = generer(tmp, base, docs)
        cas("manuel : un bloc cpp sans fichier est une erreur", code != 0 and "sans fichier" in out)
        page.write_text(texte + "\n\n```cpp fragment\nint x = 0;\n```\n", encoding="utf-8")
        code, out, site = generer(tmp, base, docs)
        cas("manuel : un « cpp fragment » passe, marqué non compilé", code == 0 and "fragment, non compilé" in lire(site, "fr/nkentseu/0.1/manuel/foundation/nkplatform/index.html"))
        page.write_text(texte, encoding="utf-8")

        # 7. version
        mut = copy.deepcopy(base)
        ex = next(x for x in mut["exemples"]["exemples"] if x["nom"] == "variables_environnement")
        ex["code"] = ex["code"].replace("env::GetEnvVar(\"PATH\")", "env::GetEnvVar(\"PATH\"); env::SetEnvVar(\"A\", \"B\")")
        code, out, site = generer(tmp, mut, docs)
        cas("version : un exemple qui emploie SetEnvVar (absent de 0.1.0-beta.11) est refusé",
            code != 0 and "SetEnvVar" in out and "absent de la version" in out)

        # 8. exemples (jenga docs exemples)
        if a.jenga:
            sys.path.insert(0, a.jenga)
        try:
            from Jenga.Commands import DocsExemples
        except Exception as exc:
            cas("exemples : module DocsExemples importable", False, str(exc))
            DocsExemples = None
        if DocsExemples:
            ex_dir = tmp / "exemples"
            shutil.copytree(DOCS / "exemples", ex_dir, copy_function=shutil.copy2)
            r0 = DocsExemples.Lancer(RACINE, ex_dir, "NkDocEx_", "Debug")
            cas("exemples : l'original est vert", r0["ok"] == r0["total"] and r0["total"] > 0, f"{r0['ok']}/{r0['total']}")
            att = next(ex_dir.rglob("ordre_des_octets.attendu"))
            st = att.stat()
            att.write_text(att.read_text(encoding="utf-8").replace("petit-boutiste", "gros-boutiste"), encoding="utf-8")
            os.utime(att, (st.st_atime, st.st_mtime))
            r1 = DocsExemples.Lancer(RACINE, ex_dir, "NkDocEx_", "Debug")
            x1 = next(x for x in r1["exemples"] if x["nom"] == "ordre_des_octets")
            cas("exemples : un attendu changé rend l'exemple rouge", x1["statut"] != "ok", x1["raison"])
            cpp = next(ex_dir.rglob("variables_environnement.cpp"))
            os.utime(cpp, None)   # la source devient plus récente que son exécutable
            r2 = DocsExemples.Lancer(RACINE, ex_dir, "NkDocEx_", "Debug")
            x2 = next(x for x in r2["exemples"] if x["nom"] == "variables_environnement")
            cas("exemples : une source plus récente que son exécutable est refusée", x2["statut"] != "ok" and "plus ancien" in x2["raison"])

    ok = all(resultats)
    print(f"banc_generateur : {'VERT' if ok else 'ROUGE'} ({sum(resultats)}/{len(resultats)} cas conformes)")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
