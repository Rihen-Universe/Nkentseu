# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# -*- coding: utf-8 -*-
"""
Le banc du site de documentation (repris de maquettes/banc_maquettes.py, 25/11/2026). Il lit le site GÉNÉRÉ :

  1. contraste WCAG AA (4,5) de 25 paires de rôles, thèmes clair et sombre (assets/site.css) ;
  2. aucune ressource distante (script, feuille, image, police, url() ou @import vers le réseau) ;
  3. ni tiret long, ni vouvoiement (pages françaises), dans les pages et leurs jumelles .md ;
  4. aucun lien mort : chaque lien relatif mène à un fichier du site, et son #ancre à un id de la page visée ;
  5. tous les exemples passent : construction.json ne porte aucune erreur, chaque exemple est « ok », et chaque bloc
     d'exemple affiché porte la marque « compilé et exécuté » ;
  6. rien de privé : aucun marqueur interne (feuille de route, checklist, suggestions, état d'agent, mandat...) ni
     chemin de la machine ;
  7. la taille : le site tient sous la limite de GitHub Pages (1 Go par site), seuil du banc 900 Mo ;
  8. l'anglais : chaque @brief de la référence a sa traduction (Docs/traductions/en/<Module>.json), pour le texte
     français ACTUEL (clé : identifiant + empreinte) ;
  9. les en-têtes restent courts (règle de Rodolf, 25/11) : par en-tête, lignes de commentaire de documentation par
     symbole public, et part des commentaires dans le fichier ; seuils annoncés ci-dessous.

--contre-epreuve rejoue le banc sur des copies MUTÉES en mémoire (une mutation par contrôle) : chacune doit rougir.

Usage (depuis la racine Nkentseu) :
  py -3 -I Docs/outils/banc_site.py --site <dossier du site> [--entetes Kernel/Foundation/NKPlatform/src/NKPlatform]
         [--contre-epreuve]
"""

import argparse
import hashlib
import json
import os
import re
import sys
from pathlib import Path

ICI = Path(__file__).resolve().parent
DOCS = ICI.parent

LIMITE_OCTETS = 900 * 1000 * 1000          # seuil du banc ; la limite de GitHub Pages est 1 Go par site
SEUIL_LIGNES_PAR_SYMBOLE = 12.0            # lignes de commentaire de documentation par symbole public, par en-tête
SEUIL_PART_COMMENTAIRES = 0.70             # part des lignes de commentaire dans un en-tête

PAIRES = [  # (texte, fond, seuil) : celles de banc_maquettes.py
    ("text", "bg", 4.5), ("text", "surface", 4.5), ("heading", "bg", 4.5), ("muted", "bg", 4.5),
    ("muted", "surface", 4.5), ("dim", "bg", 4.5), ("dim", "surface", 4.5), ("dim", "surface-2", 4.5), ("link", "bg", 4.5),
    ("link", "surface", 4.5), ("accent-text", "accent", 4.5), ("code-text", "code-bg", 4.5),
    ("tok-kw", "code-bg", 4.5), ("tok-ty", "code-bg", 4.5), ("tok-fn", "code-bg", 4.5), ("tok-str", "code-bg", 4.5),
    ("tok-com", "code-bg", 4.5), ("tok-num", "code-bg", 4.5), ("tok-pp", "code-bg", 4.5),
    ("ok", "surface", 4.5), ("warn", "surface", 4.5), ("bad", "surface", 4.5), ("exp", "surface", 4.5),
    ("muted", "surface-2", 4.5), ("text", "surface-2", 4.5),
]

PRIVES = [r"FEUILLE_DE_ROUTE", r"CHECKLIST", r"SUGGESTIONS_THEMES", r"ROADMAP", r"ORDRE_DES_TRAVAUX", r"\bETAT_\w+",
          r"\bMANDAT_\w+", r"agents_14-10", r"RihenPlatform", r"HANDOVER", r"\bDette\b", r"Nkentseu-Conception",
          r"Nkentseu-Documentation", r"[A-Za-z]:[\\/](?:Projets|Rihen|Users)\b", r"/d/Projets", r"/c/Users",
          r"AppData", r"scratchpad", r"\bcorrigés?\s+des\s+exercices"]


def lum(h):
    c = [int(h[i:i + 2], 16) / 255.0 for i in (1, 3, 5)]
    c = [x / 12.92 if x <= 0.03928 else ((x + 0.055) / 1.055) ** 2.4 for x in c]
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def contraste(a, b):
    la, lb = sorted([lum(a), lum(b)], reverse=True)
    return (la + 0.05) / (lb + 0.05)


def bloc_theme(css, nom):
    m = re.search(r'html\[data-theme="%s"\][^{]*\{([^}]*)\}' % nom, css)
    return dict(re.findall(r"--([\w-]+)\s*:\s*(#[0-9A-Fa-f]{6})", m.group(1))) if m else {}


def charger(site: Path):
    """Le site en mémoire : {chemin relatif: texte} pour .html, .md, .css, .js ; tailles de tous les fichiers."""
    textes, tailles = {}, {}
    for p in site.rglob("*"):
        if p.is_file():
            rel = p.relative_to(site).as_posix()
            tailles[rel] = p.stat().st_size
            if p.suffix in (".html", ".md", ".css", ".js", ".json") or p.name == "CNAME":
                textes[rel] = p.read_text(encoding="utf-8")
    return textes, tailles


def ids_de(t):
    return set(re.findall(r'\bid="([^"]+)"', t))


def controler(textes, tailles, entetes=None, traductions=None, apis=None, limite=LIMITE_OCTETS):
    f = []
    css = textes.get("assets/site.css", "")
    # 1. contraste
    for theme in ("sombre", "clair"):
        v = bloc_theme(css, theme)
        if not v:
            f.append(f"[contraste] thème {theme} introuvable")
            continue
        for a, b, s in PAIRES:
            if a not in v or b not in v:
                f.append(f"[contraste] {theme} : rôle manquant {a}/{b}")
            elif contraste(v[a], v[b]) < s:
                f.append(f"[contraste] {theme} : {a} sur {b} = {contraste(v[a], v[b]):.2f} < {s}")
    pages = {k: t for k, t in textes.items() if k.endswith(".html")}
    jumelles = {k: t for k, t in textes.items() if k.endswith(".md")}
    # 1 bis. versions : chaque page de la version de développement porte le bandeau, aucune page stable ne le porte
    for k, t in pages.items():
        dev = re.match(r"^(fr|en)/[^/]+/dev/", k) is not None
        if dev and 'class="note attention bandeau-dev"' not in t and "http-equiv" not in t:
            f.append(f"[bandeau] {k} : page de développement sans bandeau « non publiée »")
        if not dev and 'class="note attention bandeau-dev"' in t:
            f.append(f"[bandeau] {k} : page stable avec le bandeau de développement")
    # 2. réseau
    for k, t in list(pages.items()) + [("assets/site.css", css)]:
        for m in re.finditer(r'<(?:script|img|iframe|source|audio|video)\b[^>]*\bsrc\s*=\s*"((?:https?:)?//[^"]+)"', t):
            f.append(f"[réseau] {k} : {m.group(1)}")
        for m in re.finditer(r'<link\b[^>]*\bhref\s*=\s*"((?:https?:)?//[^"]+)"', t):
            f.append(f"[réseau] {k} : {m.group(1)}")
        for m in re.finditer(r"(?:url\(\s*['\"]?|@import\s+['\"])((?:https?:)?//[^'\")]+)", t):
            f.append(f"[réseau] {k} : {m.group(1)}")
    for k, t in textes.items():
        if k.endswith(".js") and re.search(r"\b(fetch|XMLHttpRequest|WebSocket|EventSource|sendBeacon)\b", t):
            f.append(f"[réseau] {k} : appel réseau dans un script")
    # 3. tiret long, vouvoiement
    for k, t in list(pages.items()) + list(jumelles.items()):
        corps = "\n".join(l for l in t.split("\n") if "AUTEUR" not in l)
        if "—" in corps:
            f.append(f"[tiret long] {k}")
        if k.startswith("fr/") or k == "index.html":
            texte = re.sub(r"<[^>]+>", " ", re.sub(r"<(script|style)[^>]*>.*?</\1>", " ", corps, flags=re.S))
            texte = re.sub(r"<code>.*?</code>|`[^`]*`", " ", texte)
            if re.search(r"\b(vous|votre|vos)\b", texte, flags=re.I):
                f.append(f"[vouvoiement] {k}")
    # 4. liens morts
    cache_ids = {}
    for k, t in pages.items():
        d = os.path.dirname(k)
        for m in re.finditer(r'\b(?:href|src)\s*=\s*"([^"]+)"', t):
            url = m.group(1)
            if re.match(r"^(https?:|mailto:|data:|javascript:)", url) or url.startswith("//"):
                continue
            if url.startswith("#"):
                cible, frag = k, url[1:]
            else:
                chemin, _, frag = url.partition("#")
                chemin = chemin.split("?")[0]
                if chemin.startswith("/"):
                    continue   # chemins absolus : seulement dans 404.html (redirection), vérifiés à part
                cible = os.path.normpath(os.path.join(d, chemin)).replace("\\", "/")
                if cible not in tailles:
                    f.append(f"[lien mort] {k} : {url}")
                    continue
            if frag:
                if cible not in cache_ids:
                    cache_ids[cible] = ids_de(textes.get(cible, ""))
                if frag not in cache_ids[cible]:
                    f.append(f"[ancre morte] {k} : {url}")
    # 5. exemples
    cons = json.loads(textes.get("construction.json", "{}") or "{}")
    if not cons:
        f.append("[exemples] construction.json absent")
    for x in cons.get("erreurs", []):
        f.append(f"[exemples] {x}")
    for nom, st in cons.get("exemples", {}).items():
        if st != "ok":
            f.append(f"[exemples] {nom} : {st}")
    if not cons.get("exemples"):
        f.append("[exemples] aucun exemple")
    for k, t in pages.items():
        for m in re.finditer(r'<div class="code" data-exemple="[^"]+">(.*?)</div><pre>', t, flags=re.S):
            if "badge compile" not in m.group(1):
                f.append(f"[exemples] {k} : un exemple sans la marque « compilé et exécuté »")
    # 6. privé
    for k, t in textes.items():
        for motif in PRIVES:
            m = re.search(motif, t)
            if m:
                f.append(f"[privé] {k} : « {m.group(0)} »")
    # 6 bis. licences : une police servie par le site part avec sa licence (Poppins : SIL OFL 1.1, fonts/OFL.txt)
    if any(k.startswith("assets/fonts/") and k.endswith(".woff2") for k in tailles) and "assets/fonts/OFL.txt" not in tailles:
        f.append("[licence] assets/fonts/ sert des polices sans leur licence (OFL.txt)")
    # 7. taille
    total = sum(tailles.values())
    if total > limite:
        f.append(f"[taille] {total / 1e6:.1f} Mo > {limite / 1e6:.0f} Mo")
    # 8. anglais
    if apis is not None and traductions is not None:
        for mod, api in apis.items():
            tr = traductions.get(mod, {})
            manque = [s["id"] for s in api["symboles"] if (s.get("doc") or {}).get("brief")
                      and not re.search(r"(^|::)(\w*_)?(detail|details|internal|impl)(::|$)", s.get("espace", ""))
                      and (s["id"] + "|" + hashlib.sha1(s["doc"]["brief"].strip().encode("utf-8")).hexdigest()[:10]) not in tr]
            if manque:
                f.append(f"[anglais] {mod} : {len(manque)} @brief sans traduction à jour (ex. {manque[0]})")
    # 9. en-têtes courts
    for nom, (lignes_doc, symboles, part) in (entetes or {}).items():
        lps = lignes_doc / max(symboles, 1)
        if symboles and lps > SEUIL_LIGNES_PAR_SYMBOLE:
            f.append(f"[en-tête long] {nom} : {lps:.1f} lignes de doc par symbole > {SEUIL_LIGNES_PAR_SYMBOLE}")
        if part > SEUIL_PART_COMMENTAIRES:
            f.append(f"[en-tête long] {nom} : {100 * part:.0f} % de commentaires > {100 * SEUIL_PART_COMMENTAIRES:.0f} %")
    return f, total


def mesurer_entetes(dossier: Path, api):
    """Par en-tête : (lignes de commentaire de documentation, symboles publics de api.json, part des commentaires)."""
    res = {}
    for h in sorted(dossier.glob("*.h")):
        t = h.read_text(encoding="utf-8", errors="replace")
        lignes = t.split("\n")
        doc = 0
        com = 0
        dans = False
        dans_doc = False
        for l in lignes:
            s = l.strip()
            if dans:
                com += 1
                doc += 1 if dans_doc else 0
                if "*/" in s:
                    dans = False
                continue
            if s.startswith("/*"):
                dans_doc = s.startswith("/**") or s.startswith("/*!")
                com += 1
                doc += 1 if dans_doc else 0
                dans = "*/" not in s[2:]
            elif s.startswith("//"):
                com += 1
                doc += 1 if (s.startswith("///") or s.startswith("//!")) else 0
        n = sum(1 for s in api["symboles"] if os.path.basename(s["fichier"]) == h.name) if api else 0
        res[h.name] = (doc, n, com / max(len(lignes), 1))
    return res


def main():
    # la console Windows est en cp1252 (et -I ignore PYTHONIOENCODING) : les messages sortent en UTF-8
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser()
    ap.add_argument("--site", required=True)
    ap.add_argument("--construction", help="Dossier api/ (pour la couverture de l'anglais)")
    ap.add_argument("--entetes", action="append", default=[], help="Dossier d'en-têtes à mesurer (règle « en-têtes courts »)")
    ap.add_argument("--contre-epreuve", action="store_true")
    a = ap.parse_args()
    site = Path(a.site)
    textes, tailles = charger(site)
    apis, traductions = None, None
    if a.construction:
        apis, traductions = {}, {}
        # chaque version affichée (api/<version>/<Module>/api.json) ; « main » ne sert qu'à la garde des exemples
        for p in sorted((Path(a.construction) / "api").glob("*/*/api.json")):
            if p.parent.parent.name == "main":
                continue
            mod = p.parent.name
            apis[p.parent.parent.name + "/" + mod] = json.loads(p.read_text(encoding="utf-8"))
            tf = DOCS / "traductions" / "en" / f"{mod}.json"
            traductions[p.parent.parent.name + "/" + mod] = json.loads(tf.read_text(encoding="utf-8")).get("entrees", {}) if tf.exists() else {}
    entetes = {}
    for d in a.entetes:
        mod = Path(d).name
        api_dev = (apis or {}).get("dev/" + mod) or next((v for k, v in (apis or {}).items() if k.endswith("/" + mod)), None)
        entetes.update(mesurer_entetes(Path(d), api_dev))
    fautes, total = controler(textes, tailles, entetes, traductions, apis)
    for x in fautes[:60]:
        print("ROUGE " + x)
    if len(fautes) > 60:
        print(f"... et {len(fautes) - 60} autre(s)")
    nb_pages = sum(1 for k in textes if k.endswith(".html"))
    print(f"banc_site : {'VERT' if not fautes else 'ROUGE'} ({len(fautes)} faute(s) ; {nb_pages} pages ; "
          f"{total / 1e6:.2f} Mo sur {LIMITE_OCTETS / 1e6:.0f} Mo permis ; {len(PAIRES)} paires x 2 thèmes)")
    for nom, (doc, n, part) in entetes.items():
        print(f"  en-tête {nom:28s} : {doc:5d} lignes de doc, {n:4d} symboles, {doc / max(n, 1):5.1f} par symbole, "
              f"{100 * part:3.0f} % de commentaires")
    if not a.contre_epreuve:
        return 1 if fautes else 0

    def muter(k, avant, apres):
        t = dict(textes)
        t[k] = t[k].replace(avant, apres, 1)
        return t
    page_fr = "fr/nkentseu/0.1/manuel/foundation/nkplatform/index.html"
    cons = json.loads(textes["construction.json"])
    cons_mut = dict(cons, exemples=dict(cons["exemples"], **{next(iter(cons["exemples"])): "echec"}))
    mutations = {
        "contraste": (muter("assets/site.css", "--muted: #a3aeb5", "--muted: #3a4046"), tailles, {}),
        "réseau": (muter(page_fr, "</head>", '<script src="https://exemple.invalid/x.js"></script></head>'), tailles, {}),
        "tiret long": (muter(page_fr, "<h1>", "<h1>— "), tailles, {}),
        "vouvoiement": (muter(page_fr, "<h1>", "<h1>Vous "), tailles, {}),
        "lien mort": (muter(page_fr, "</main>", '<a href="../absente/index.html">x</a></main>'), tailles, {}),
        "ancre morte": (muter(page_fr, "</main>", '<a href="#nulle-part">x</a></main>'), tailles, {}),
        "exemple en échec": (dict(textes, **{"construction.json": json.dumps(cons_mut)}), tailles, {}),
        "marqueur privé": (muter(page_fr, "</main>", "<p>voir docs/FEUILLE_DE_ROUTE.md</p></main>"), tailles, {}),
        "taille": (textes, dict(tailles, **{"gros.bin": 950 * 1000 * 1000}), {}),
        "licence absente": (textes, {k: v for k, v in tailles.items() if k != "assets/fonts/OFL.txt"}, {}),
        "bandeau absent": (muter("fr/nkentseu/dev/index.html", 'class="note attention bandeau-dev"', 'class="note"'), tailles, {}),
        "bandeau en stable": (muter(page_fr, "</main>", '<div class="note attention bandeau-dev">x</div></main>'), tailles, {}),
    }
    if apis:
        mod = next(iter(apis))
        tr_mut = {m: dict(v) for m, v in traductions.items()}
        # retirer la traduction d'un @brief que CETTE version affiche vraiment
        s0 = next(s for s in apis[mod]["symboles"] if (s.get("doc") or {}).get("brief"))
        cle = s0["id"] + "|" + hashlib.sha1(s0["doc"]["brief"].strip().encode("utf-8")).hexdigest()[:10]
        tr_mut.setdefault(mod, {}).pop(cle, None)
        mutations["anglais"] = (textes, tailles, {"traductions": tr_mut})
    if entetes:
        nom0 = next(iter(entetes))
        doc, n, part = entetes[nom0]
        mutations["en-tête long"] = (textes, tailles, {"entetes": dict(entetes, **{nom0: (doc + 100 * max(n, 1), n, 0.95)})})
    ratees = 0
    for nom, (t, ta, extra) in mutations.items():
        fm, _ = controler(t, ta, extra.get("entetes", entetes), extra.get("traductions", traductions), apis)
        nouvelles = [x for x in fm if x not in fautes]
        ok = bool(nouvelles)
        ratees += 0 if ok else 1
        print(f"contre-épreuve {nom:18s} : {'ROUGIT comme prévu' if ok else 'NE ROUGIT PAS'} ({len(nouvelles)} nouvelle(s))")
    print("contre-épreuves : " + ("TOUTES ROUGISSENT" if not ratees else f"{ratees} RATÉE(S)"))
    return 1 if (fautes or ratees) else 0


if __name__ == "__main__":
    sys.exit(main())
