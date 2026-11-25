# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# -*- coding: utf-8 -*-
"""
Le générateur du site de documentation (docs.rihen-universe.com), sans dépendance hors de Python.

Il lit seulement une LISTE BLANCHE de sources :
  - Docs/site/site.json            : les projets, familles, versions, couches ;
  - Docs/site/<langue>/<projet>/   : les pages écrites (Markdown simple, en-tête « --- clé: valeur --- ») ;
  - Docs/theme/                    : la feuille de style, les polices et les logos (copiés en local) ;
  - Docs/traductions/en/*.json     : l'anglais des descriptions de la référence ;
  - Docs/exemples/ (par exemples.json, écrit par « jenga docs exemples ») ;
  - api/<Module>/api.json et graphe.json (écrits par « jenga docs api --etiquette » et « jenga docs graphe »).
Il écrit le site statique : le portail, l'accueil de chaque projet, les quatre espaces, la référence, une jumelle
.md par page, l'index de recherche par langue et par version, les diagrammes en SVG.

Règles tenues ici (le banc du site les vérifie sur la sortie) :
  - un bloc ```cpp sans fichier derrière est une ERREUR, sauf « ```cpp fragment » (affiché « fragment, non compilé ») ;
  - un exemple montré l'est avec la sortie de la construction, jamais recopiée, et seulement s'il a réussi ;
  - un nom cité par [[Nom]] doit exister dans api.json ; un lien @/chemin doit mener à une page ;
  - le tiret long des commentaires devient deux-points.

Usage (depuis la racine Nkentseu) :
  py -3 Docs/outils/generer_site.py --construction <dossier api/graphe/exemples> --sortie <dossier du site>
Code 1 à la première erreur de contenu (avec la liste), 0 sinon.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import os
import re
import shutil
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

ICI = Path(__file__).resolve().parent
DOCS = ICI.parent

UI = {
    "fr": {
        "doc": "Documentation", "rechercher": "Rechercher dans la doc", "version": "Version", "derniere": "dernière",
        "derniere_pub": "dernière publiée", "local": "en local seulement", "theme": "Changer de thème",
        "espaces": "Espaces", "sur_page": "Sur cette page", "inclure": "Inclure", "espace_noms": "Espace de noms",
        "lier": "Lier (Jenga)", "genere": "Générée par jenga docs depuis l'étiquette", "source": "Source",
        "ligne": "ligne", "md": "Cette page en Markdown", "signaler": "Signaler un problème",
        "compile": "compilé et exécuté", "fragment": "fragment, non compilé", "sortie": "Sortie", "copier": "Copier",
        "parametres": "Paramètres", "nom": "Nom", "type": "Type", "role": "Rôle", "defaut": "Défaut",
        "retour": "Retour", "voir": "Voir aussi", "attention": "Attention", "note": "Note", "deprecie": "Déprécié",
        "membres": "Membres", "details": "Détail des membres", "types": "Types", "fonctions": "Fonctions",
        "macros": "Macros", "alias": "Alias", "variables": "Variables", "enum": "Énumération", "valeurs": "Valeurs",
        "couverture": "Symboles publics décrits (@brief)", "entetes": "En-têtes", "hierarchie": "Hiérarchie",
        "bases": "Classes de base", "derivees": "Classes dérivées", "aucune": "aucune",
        "depend_de": "Dépend de", "utilise_par": "Utilisé par", "dependances": "Dépendances (lues dans les .jenga)",
        "a_venir": "à venir", "portail_titre": "La documentation de Rihen",
        "portail_sous": "Les projets de Rihen, documentés pour celles et ceux qui les utilisent.",
        "principal": "Le projet principal", "ouvrir": "Ouvrir la documentation", "reference": "Référence",
        "module": "Module", "couche_titre": "Le moteur, couche par couche",
        "couche_sous": "Une couche ne dépend que de celles du dessus", "sante": "Santé de la doc",
        "exemples_ok": "Exemples compilés et exécutés", "traduit": "Descriptions traduites en anglais",
        "non_traduit": "Pas encore traduit : le texte français s'affiche.", "option": "option à définir par toi",
        "modele": "générique (template)", "commencer": "Commencer", "suivant": "Suivant", "precedent": "Précédent",
        "graphe_couches": "Le graphe des couches (tiré des .jenga)", "rien": "Rien pour",
        "accueil": "Accueil", "tout": "Tout", "manuel": "Manuel", "guides": "Guides", "tutoriels": "Tutoriels",
        "echap": "Échap", "choisir": "choisir", "entree": "Entrée", "ouvrir_r": "ouvrir",
        "hors_ligne": "Hors ligne", "pour_utiliser": "Pour l'utiliser", "resume_module": "Ce que fait ce module",
        "langue": "Langue",
        "dev_titre": "Version de développement, non publiée.",
        "dev_texte": " Construite depuis main : elle montre ce qui vient (corrections, descriptions écrites depuis la dernière étiquette) et peut changer à chaque commit.",
        "vers_stable": "Aller à la version stable",
    },
    "en": {
        "doc": "Documentation", "rechercher": "Search the docs", "version": "Version", "derniere": "latest",
        "derniere_pub": "latest published", "local": "local only", "theme": "Switch theme",
        "espaces": "Spaces", "sur_page": "On this page", "inclure": "Include", "espace_noms": "Namespace",
        "lier": "Link (Jenga)", "genere": "Generated by jenga docs from tag", "source": "Source",
        "ligne": "line", "md": "This page in Markdown", "signaler": "Report a problem",
        "compile": "compiled and run", "fragment": "fragment, not compiled", "sortie": "Output", "copier": "Copy",
        "parametres": "Parameters", "nom": "Name", "type": "Type", "role": "Role", "defaut": "Default",
        "retour": "Returns", "voir": "See also", "attention": "Warning", "note": "Note", "deprecie": "Deprecated",
        "membres": "Members", "details": "Member details", "types": "Types", "fonctions": "Functions",
        "macros": "Macros", "alias": "Aliases", "variables": "Variables", "enum": "Enumeration", "valeurs": "Values",
        "couverture": "Public symbols described (@brief)", "entetes": "Headers", "hierarchie": "Hierarchy",
        "bases": "Base classes", "derivees": "Derived classes", "aucune": "none",
        "depend_de": "Depends on", "utilise_par": "Used by", "dependances": "Dependencies (read from the .jenga files)",
        "a_venir": "coming soon", "portail_titre": "Rihen documentation",
        "portail_sous": "Rihen's projects, documented for the people who use them.",
        "principal": "The main project", "ouvrir": "Open the documentation", "reference": "Reference",
        "module": "Module", "couche_titre": "The engine, layer by layer",
        "couche_sous": "A layer only depends on the ones above it", "sante": "Health of the docs",
        "exemples_ok": "Examples compiled and run", "traduit": "Descriptions translated into English",
        "non_traduit": "Not translated yet: the French text is shown.", "option": "option you define yourself",
        "modele": "generic (template)", "commencer": "Start", "suivant": "Next", "precedent": "Previous",
        "graphe_couches": "The layer graph (read from the .jenga files)", "rien": "Nothing for",
        "accueil": "Home", "tout": "All", "manuel": "Manual", "guides": "Guides", "tutoriels": "Tutorials",
        "echap": "Esc", "choisir": "choose", "entree": "Enter", "ouvrir_r": "open",
        "hors_ligne": "Offline", "pour_utiliser": "How to use it", "resume_module": "What this module does",
        "langue": "Language",
        "dev_titre": "Development version, not published.",
        "dev_texte": " Built from main: it shows what is coming (fixes, descriptions written since the last tag) and can change with every commit.",
        "vers_stable": "Go to the stable version",
    },
}

NATURES = {
    "fr": {"classe": "classe", "struct": "structure", "union": "union", "enum": "énumération", "fonction": "fonction",
           "methode": "méthode", "constructeur": "constructeur", "destructeur": "destructeur",
           "conversion": "conversion", "alias": "alias", "variable": "variable", "champ": "champ", "macro": "macro"},
    "en": {"classe": "class", "struct": "struct", "union": "union", "enum": "enumeration", "fonction": "function",
           "methode": "method", "constructeur": "constructor", "destructeur": "destructor",
           "conversion": "conversion", "alias": "alias", "variable": "variable", "champ": "field", "macro": "macro"},
}

ESPACE_REPERTOIRE = {"apprendre": "T", "manuel": "M", "guides": "G", "reference": "R"}


class ErreurContenu(Exception):
    pass


def e(s: Any) -> str:
    return html.escape("" if s is None else str(s), quote=True)


def tiret(s: Optional[str]) -> str:
    """Le tiret long d'un commentaire d'en-tête devient deux-points (règle de la documentation)."""
    return (s or "").replace(" \u2014 ", " : ").replace("\u2014", ":")


# Un paragraphe de commentaire qui raconte l'histoire interne (une date, un banc, une décision, un fichier de suivi)
# n'est pas de la documentation publique : il reste dans l'en-tête, il ne va pas sur le site. Le compte des
# paragraphes écartés est écrit dans construction.json (rien n'est retiré en silence).
_INTERNE = re.compile(r"\bRodolf\b|ROADMAP|FEUILLE_DE_ROUTE|\bbancs?\b|contre-?[ée]preuve|NK_NKCODE_MUTATION|"
                      r"\b20\d\d-\d\d-\d\d\b|\(\d\d/\d\d(/20\d\d)?|\bagent\b", re.I)
ECARTES: List[str] = []


def public(paras: List[str]) -> List[str]:
    garde = []
    for p in paras:
        if _INTERNE.search(p or ""):
            ECARTES.append(p[:80])
        else:
            garde.append(p)
    return garde


def texte_inline(s: Optional[str]) -> str:
    """Texte d'un commentaire d'en-tête vers HTML : échappé, `code` reconnu, tiret long converti."""
    s = e(tiret(s))
    return re.sub(r"`([^`]+)`", r"<code>\1</code>", s)


def ancre(s: str) -> str:
    # « Résumé » donne « resume », pas « r-sum » : les accents sont retirés avant de remplacer le reste
    import unicodedata
    s = "".join(c for c in unicodedata.normalize("NFD", s) if unicodedata.category(c) != "Mn").lower()
    s = re.sub(r"[^a-z0-9]+", "-", s.replace("~", "dtor-").replace("operator", "op-"))
    return s.strip("-") or "x"


def empreinte(s: str) -> str:
    return hashlib.sha1((s or "").encode("utf-8")).hexdigest()[:10]


# ---------------------------------------------------------------------------------------------------------------
# Coloration du C++ (les classes de la feuille : pp, kw, ty, fn, st, nu, cm)
# ---------------------------------------------------------------------------------------------------------------

_MOTS = set("""alignas alignof auto bool break case catch char char8_t char16_t char32_t class const consteval constexpr
constinit const_cast continue co_await co_return co_yield decltype default delete do double dynamic_cast else enum
explicit export extern false float for friend goto if inline int long mutable namespace new noexcept nullptr operator
private protected public register reinterpret_cast requires return short signed sizeof static static_assert
static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using
virtual void volatile wchar_t while""".split())
_TYPES = set("int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t size_t NkSize".split())
_JETON = re.compile(r"(?P<cm>//[^\n]*|/\*.*?\*/)|(?P<st>\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*')|(?P<pp>^[ \t]*#[ \t]*[a-z]+)"
                    r"|(?P<nu>\b0[xX][0-9a-fA-F']+[uUlL]*\b|\b\d[\d']*(?:\.\d+)?[uUlLfF]*\b)|(?P<id>[A-Za-z_][A-Za-z0-9_]*)",
                    re.S | re.M)


def colorer_cpp(code: str) -> str:
    out = []
    pos = 0
    for m in _JETON.finditer(code):
        out.append(e(code[pos:m.start()]))
        g = m.lastgroup
        t = m.group(0)
        if g == "id":
            suite = code[m.end():m.end() + 2].lstrip()
            if t in _MOTS:
                cls = "kw"
            elif t in _TYPES or re.match(r"^(Nk|NK)[A-Z]\w*$", t) and not code[m.end():].lstrip().startswith("("):
                cls = "ty"
            elif code[m.end():].lstrip().startswith("("):
                cls = "fn"
            else:
                cls = ""
            out.append(f'<span class="{cls}">{e(t)}</span>' if cls else e(t))
        else:
            out.append(f'<span class="{g}">{e(t)}</span>')
        pos = m.end()
    out.append(e(code[pos:]))
    return "".join(out)


# ---------------------------------------------------------------------------------------------------------------
# Le site : chemins relatifs, pages, index de recherche
# ---------------------------------------------------------------------------------------------------------------

class Site:
    def __init__(self, sortie: Path, config: Dict[str, Any], construction: Path):
        self.sortie = sortie
        self.config = config
        self.construction = construction
        self.pages: Dict[str, Dict[str, Any]] = {}     # chemin relatif -> infos
        self.index: Dict[Tuple[str, str, str], List[Dict[str, str]]] = {}
        self.erreurs: List[str] = []
        self.apis: Dict[str, Dict[str, Any]] = {}
        self.apis_main: Dict[str, Dict[str, Any]] = {}
        self.apis_par_version: Dict[str, Dict[str, Dict[str, Any]]] = {}
        self.traductions: Dict[str, Dict[str, str]] = {}
        self.manques_trad: Dict[str, int] = {}
        self.exemples: Dict[str, Dict[str, Any]] = {}
        self.graphe: Dict[str, Any] = {}

    # --- chemins -------------------------------------------------------------------------------------------------
    @staticmethod
    def rel(depuis: str, vers: str) -> str:
        """Lien relatif de la page `depuis` (chemin de fichier dans le site) vers `vers` (fichier ou dossier/)."""
        if vers.endswith("/"):
            vers += "index.html"
        frag = ""
        if "#" in vers:
            vers, frag = vers.split("#", 1)
            frag = "#" + frag
        d = os.path.dirname(depuis)
        r = os.path.relpath(vers, d if d else ".").replace("\\", "/")
        return r + frag

    def ecrire(self, chemin: str, contenu: str, md: Optional[str] = None) -> None:
        p = self.sortie / chemin
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(contenu, encoding="utf-8")
        if md is not None:
            p.with_suffix(".md").write_text(md, encoding="utf-8")
        self.pages[chemin] = {"md": md is not None}

    def indexer(self, langue: str, projet: str, version: str, k: str, t: str, c: str, u: str, m: str = "") -> None:
        self.index.setdefault((langue, projet, version), []).append({"k": k, "t": t, "c": c, "u": u, "m": m})


# ---------------------------------------------------------------------------------------------------------------
# Le gabarit commun
# ---------------------------------------------------------------------------------------------------------------

ICONES = {
    "recherche": '<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" aria-hidden="true"><circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/></svg>',
    "lune": '<svg class="ic-lune" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8Z"/></svg>',
    "soleil": '<svg class="ic-soleil" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>',
    "apprendre": '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M4 19V6a2 2 0 0 1 2-2h12v15H6a2 2 0 0 0-2 2"/><path d="M9 8h6"/></svg>',
    "manuel": '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M12 8v5M12 16h.01"/></svg>',
    "guides": '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="m14.7 6.3 3 3L9 18H6v-3l8.7-8.7Z"/><path d="m13 8 3 3"/></svg>',
    "reference": '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="m8 7-5 5 5 5M16 7l5 5-5 5"/></svg>',
}


def gabarit(site: Site, chemin: str, langue: str, titre: str, corps: str, projet: Optional[Dict[str, Any]] = None,
            version: Optional[Dict[str, Any]] = None, espace_actif: str = "", toc: Optional[List[Tuple[str, str, int]]] = None,
            arbre: str = "", md_lien: bool = True, classe_main: str = "") -> str:
    u = UI[langue]
    autre = "en" if langue == "fr" else "fr"
    racine_site = Site.rel(chemin, "index.html").rsplit("index.html", 1)[0] or "./"
    assets = Site.rel(chemin, "assets/site.css").rsplit("site.css", 1)[0]
    # la même page dans l'autre langue : seul le préfixe change
    if chemin.startswith(langue + "/"):
        jumeau = autre + "/" + chemin[len(langue) + 1:]
    else:
        jumeau = autre + "/index.html"
    lien_autre = Site.rel(chemin, jumeau)
    lien_cette = Site.rel(chemin, chemin)
    nav = ""
    racine_version = ""
    if projet and version:
        base = f"{langue}/{projet['id']}/{version['version']}/"
        racine_version = Site.rel(chemin, base + "index.html").rsplit("index.html", 1)[0] or "./"
        liens = []
        for esp in site.config["genres"][projet.get("genre", "moteur")]["espaces"]:
            cible = base + esp["id"] + "/index.html"
            cls = ' class="actif" aria-current="page"' if esp["id"] == espace_actif else ""
            liens.append(f'<a href="{Site.rel(chemin, cible)}"{cls}>{e(esp[langue])}</a>')
        nav = f'<nav class="espaces" aria-label="{u["espaces"]}">{"".join(liens)}</nav>'
        nom_marque = projet["nom"]
        lien_marque = Site.rel(chemin, base + "index.html")
    else:
        nom_marque = "Rihen"
        lien_marque = Site.rel(chemin, f"{langue}/index.html")
    # l'index de recherche : celui de la version affichée, sinon celui de la dernière version de Nkentseu
    if not racine_version:
        p0 = next(p for p in site.config["projets"] if p.get("principal"))
        v0 = next(v for v in p0["versions"] if v.get("derniere"))
        racine_version = Site.rel(chemin, f"{langue}/{p0['id']}/{v0['version']}/index.html").rsplit("index.html", 1)[0]
        vlib = v0["version"]
    else:
        vlib = version["version"]
    menu_version = ""
    if projet and version:
        items = []
        for v in projet["versions"]:
            cible = chemin.replace(f"/{version['version']}/", f"/{v['version']}/", 1)
            cls = ' class="courant"' if v["version"] == version["version"] else ""
            note = f' <span class="mnote">{u["derniere_pub"]}</span>' if v.get("derniere") else ""
            items.append(f'<a{cls} href="{Site.rel(chemin, cible)}">{e(libelle_version(v))}{note}</a>')
        if not any(v.get("publiee") is False for v in projet["versions"]):
            items.append(f'<hr><span class="off">main <span class="mnote">{u["local"]}</span></span>')
        past = f' <span class="pastille">{u["derniere"]}</span>' if version.get("derniere") else ""
        menu_version = (f'<div class="choix version"><button aria-haspopup="true" aria-expanded="false">'
                        f'<span class="etiq">{u["version"]}</span> {e(libelle_version(version))}{past} ▾</button>'
                        f'<div class="menu" role="menu">{"".join(items)}</div></div>')
    toc_html = ""
    if toc:
        toc_html = (f'<aside class="toc" aria-label="{u["sur_page"]}"><div class="t">{u["sur_page"]}</div>'
                    + "".join(f'<a class="n2" href="#{a}">{e(t)}</a>' if n > 2 else f'<a href="#{a}">{e(t)}</a>'
                              for a, t, n in toc) + "</aside>")
    if version and version.get("publiee") is False:
        # chaque page de la version de développement le dit, avec un lien vers la même page de la stable
        stable = next((v for v in projet["versions"] if v.get("derniere")), None)
        lien_stable = ""
        if stable:
            cible = chemin.replace(f"/{version['version']}/", f"/{stable['version']}/", 1)
            if cible not in site.pages:
                cible = f"{langue}/{projet['id']}/{stable['version']}/index.html"
            lien_stable = f' <a href="{Site.rel(chemin, cible)}">{u["vers_stable"]} {e(stable["etiquette"].lstrip("v"))}</a>'
        corps = f'<div class="note attention bandeau-dev"><span class="nt">{u["dev_titre"]}</span>{u["dev_texte"]}{lien_stable}</div>' + corps
    if langue == "en" and ' lang="fr"' in corps:
        # jamais un anglais périmé ni inventé : le français s'affiche, et la page le dit
        corps = f'<div class="note bandeau-trad"><span class="nt">{u["non_traduit"]}</span></div>' + corps
    entete = f"""<header class="entete">
  <a class="marque" href="{lien_marque}">
    <img class="logo-sombre" src="{assets}brand/logo_horizontal_blanc.svg" alt="Rihen">
    <img class="logo-clair" src="{assets}brand/logo_horizontal_couleur.svg" alt="Rihen">
    <span class="sep"></span>
    <span class="nom">{e(nom_marque)}<small>{u["doc"]}</small></span>
  </a>
  {nav}
  <div class="droite">
    <button class="btn-recherche" data-recherche aria-label="{u["rechercher"]}">{ICONES["recherche"]}<span class="txt">{u["rechercher"]}</span><span class="kbds"><kbd>Ctrl</kbd><kbd>K</kbd></span></button>
    {menu_version}
    <div class="langues" role="group" aria-label="{u["langue"]}"><a data-l="fr" href="{lien_cette if langue == "fr" else lien_autre}" aria-current="{str(langue == "fr").lower()}" lang="fr">FR</a><a data-l="en" href="{lien_cette if langue == "en" else lien_autre}" aria-current="{str(langue == "en").lower()}" lang="en">EN</a></div>
    <button class="theme" title="{u["theme"]}" aria-label="{u["theme"]}">{ICONES["lune"]}{ICONES["soleil"]}</button>
  </div>
</header>"""
    filtres = "".join(f'<button data-k="{k}" aria-pressed="{"true" if k == "" else "false"}">{e(lib)}</button>'
                      for k, lib in (("", u["tout"]), ("R", u["reference"]), ("M", u["manuel"]), ("G", u["guides"]),
                                     ("T", u["tutoriels"])))
    recherche = f"""<div class="voile" role="dialog" aria-label="{u["rechercher"]}">
  <div class="dialogue">
    <div class="champ">{ICONES["recherche"]}<input type="search" placeholder="{u["rechercher"]}" aria-label="{u["rechercher"]}"><kbd>{u["echap"]}</kbd></div>
    <div class="filtres">{filtres}</div>
    <div class="resultats"></div>
    <div class="bas"><span><kbd>↑</kbd> <kbd>↓</kbd> {u["choisir"]}</span><span><kbd>{u["entree"]}</kbd> {u["ouvrir_r"]}</span><span>{u["hors_ligne"]} · {e(vlib)}</span></div>
  </div>
</div>"""
    if arbre or toc_html:
        contenu = f'<div class="cadre">{arbre}<main class="contenu {classe_main}"><div class="large">{corps}</div></main>{toc_html}</div>'
    else:
        contenu = f'<main class="{classe_main or "accueil"}">{corps}</main>'
    return f"""<!DOCTYPE html>
<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis, Rihen. Page générée par Docs/outils/generer_site.py : ne pas modifier à la main. -->
<html lang="{langue}" data-racine="{e(racine_version)}" data-version="{e(vlib)}" data-rien="{e(u["rien"])}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{e(titre)}</title>
<link rel="stylesheet" href="{assets}site.css">
<script src="{assets}site.js"></script>
<script src="{racine_version}recherche.js" defer></script>
</head>
<body>
{entete}
{contenu}
{recherche}
</body>
</html>
"""


def libelle_version(v: Dict[str, Any]) -> str:
    """« 0.1.0-beta.11 » pour une étiquette, « dev (main) » pour la version de développement."""
    return "dev (main)" if v.get("etiquette") == "main" else v["etiquette"].lstrip("v")


def pied(site: Site, langue: str, chemin: str, extra: str = "", etiquette: str = "", md: bool = True) -> str:
    u = UI[langue]
    bits = ["<span>© Rihen</span>"]
    if etiquette:
        bits.append(f'<span>{u["genere"]} <code>{e(etiquette)}</code></span>')
    if md:
        bits.append(f'<a href="{e(os.path.basename(chemin)[:-5] + ".md")}">{u["md"]}</a>')
    bits.append(f'<a href="{e(site.config["depot"])}/issues">{u["signaler"]}</a>')
    if extra:
        bits.append(extra)
    return f'<footer class="pied-page">{"".join(bits)}</footer>'


# ---------------------------------------------------------------------------------------------------------------
# Markdown des pages écrites
# ---------------------------------------------------------------------------------------------------------------

def lire_page(p: Path) -> Tuple[Dict[str, str], str]:
    t = p.read_text(encoding="utf-8").replace("\r\n", "\n")
    meta: Dict[str, str] = {}
    if t.startswith("---\n"):
        fin = t.index("\n---\n", 4)
        for l in t[4:fin].split("\n"):
            if ":" in l:
                k, v = l.split(":", 1)
                meta[k.strip()] = v.strip()
        t = t[fin + 5:]
    t = re.sub(r"^<!--.*?-->\n", "", t, flags=re.S)
    return meta, t


class Rendu:
    """Markdown simple vers HTML, avec les directives de la doc : {{exemple:...}}, {{extrait:...#...}}, [[Symbole]],
    liens @/chemin (relatifs à la racine de la version)."""

    def __init__(self, site: Site, chemin: str, langue: str, base_version: str, projet_id: str, version: str):
        self.site = site
        self.chemin = chemin
        self.langue = langue
        self.base = base_version
        self.projet = projet_id
        self.version = version
        self.toc: List[Tuple[str, str, int]] = []
        self.erreurs: List[str] = []
        self.liens_internes: List[str] = []

    def inline(self, s: str) -> str:
        codes: List[str] = []

        def garder(m):
            codes.append(f"<code>{e(m.group(1))}</code>")
            return f"\x00{len(codes) - 1}\x00"
        s = re.sub(r"`([^`]+)`", garder, s)
        s = e(s)

        def symbole(m):
            nom = html.unescape(m.group(1))
            cible = self.site_symbole(nom)
            if not cible:
                self.erreurs.append(f"{self.chemin} : [[{nom}]] ne nomme aucun symbole de la référence")
                return f"<code>{e(nom)}</code>"
            return f'<a href="{Site.rel(self.chemin, cible)}"><code>{e(nom)}</code></a>'
        s = re.sub(r"\[\[([^\]]+)\]\]", symbole, s)

        def lien(m):
            txt, url = m.group(1), html.unescape(m.group(2))
            if url.startswith("@/"):
                cible = self.base + url[2:].strip("/") + "/index.html"
                if "#" in url:
                    p, f = url[2:].split("#", 1)
                    cible = self.base + p.strip("/") + "/index.html#" + f
                self.liens_internes.append(cible.split("#")[0])
                url = Site.rel(self.chemin, cible)
            return f'<a href="{e(url)}">{txt}</a>'
        s = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", lien, s)
        s = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", s)
        s = re.sub(r"(?<![\w*])\*([^*\s][^*]*?)\*(?![\w*])", r"<em>\1</em>", s)
        s = re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], s)
        return s

    def site_symbole(self, nom: str) -> Optional[str]:
        for mod, api in self.site.apis.items():
            for s in api["symboles"]:
                if s["qualifie"] == nom or s["nom"] == nom or s["qualifie"].endswith("::" + nom):
                    return lien_symbole(self.base, mod, api, s)
        return None

    def bloc_exemple(self, ref: str, extrait: Optional[str]) -> str:
        u = UI[self.langue]
        ex = self.site.exemples.get(ref)
        if ex is None:
            self.erreurs.append(f"{self.chemin} : l'exemple {ref} n'existe pas dans exemples.json")
            return ""
        if ex["statut"] != "ok":
            self.erreurs.append(f"{self.chemin} : l'exemple {ref} n'a pas réussi ({ex['raison']}) : on ne le montre pas")
            return ""
        self.verifier_version(ref, ex)
        if extrait:
            if extrait not in ex["extraits"]:
                self.erreurs.append(f"{self.chemin} : l'extrait {extrait} n'existe pas dans {ref}")
                return ""
            code = ex["extraits"][extrait]
        else:
            code = ex["code"]
        sortie = "" if extrait else (f'<div class="sortie"><div class="et">{u["sortie"]}</div>'
                                     f'<pre>{e(ex["sortie"].rstrip())}</pre></div>')
        return (f'<div class="code" data-exemple="{e(ref)}"><div class="barre"><span class="fichier">{e(ex["fichier"])}'
                f'{(" · " + e(extrait)) if extrait else ""}</span><span class="droite-b"><span class="badge compile">✓ '
                f'{u["compile"]}</span><button data-copier>{u["copier"]}</button></span></div>'
                f'<pre>{colorer_cpp(code)}</pre>{sortie}</div>')

    def verifier_version(self, ref: str, ex: Dict[str, Any]) -> None:
        """Un exemple est construit contre l'arbre de travail, mais il est montré dans la doc d'une version publiée :
        chaque nom de l'API qu'il emploie doit exister À L'ÉTIQUETTE, avec les mêmes signatures. Sinon, l'exemple
        mentirait sur la version (cas réel : SetEnvVar, ajouté le 19/10, absent de 0.1.0-beta.11)."""
        mod = ex["module"]
        tag, main = self.site.apis.get(mod), self.site.apis_main.get(mod)
        if not tag or not main:
            return
        sig_tag: Dict[str, set] = {}
        sig_main: Dict[str, set] = {}
        for s in tag["symboles"]:
            sig_tag.setdefault(s["nom"], set()).add(s.get("signature", ""))
        for s in main["symboles"]:
            sig_main.setdefault(s["nom"], set()).add(s.get("signature", ""))
        for ident in sorted(set(re.findall(r"\b[A-Za-z_]\w*\b", ex["code"]))):
            if ident not in sig_main:
                continue
            if ident not in sig_tag:
                self.erreurs.append(f"{self.chemin} : l'exemple {ref} emploie {ident}, absent de la version {self.version}")
            elif sig_tag[ident] != sig_main[ident] and not (sig_tag[ident] & sig_main[ident]):
                self.erreurs.append(f"{self.chemin} : l'exemple {ref} emploie {ident}, dont la signature a changé depuis "
                                    f"la version {self.version}")

    def rendre(self, texte: str) -> str:
        u = UI[self.langue]
        out: List[str] = []
        lignes = texte.split("\n")
        i = 0
        para: List[str] = []

        def fin_para():
            if para:
                out.append(f"<p>{self.inline(' '.join(para))}</p>")
                para.clear()
        while i < len(lignes):
            l = lignes[i]
            s = l.strip()
            m = re.match(r"^(#{1,4})\s+(.*)$", s)
            if m:
                fin_para()
                n = len(m.group(1))
                titre = m.group(2)
                a = ancre(titre)
                if n >= 2:
                    self.toc.append((a, re.sub(r"[`*]", "", titre), n))
                out.append(f'<h{n} id="{a}">{self.inline(titre)}</h{n}>')
                i += 1
                continue
            m = re.match(r"^\{\{(exemple|extrait):([^}#]+)(?:#([^}]+))?\}\}$", s)
            if m:
                fin_para()
                out.append(self.bloc_exemple(m.group(2).strip(), m.group(3)))
                i += 1
                continue
            if s.startswith("```"):
                fin_para()
                info = s[3:].strip().split()
                lang = info[0] if info else ""
                j = i + 1
                bloc = []
                while j < len(lignes) and not lignes[j].strip().startswith("```"):
                    bloc.append(lignes[j])
                    j += 1
                code = "\n".join(bloc)
                if lang in ("cpp", "c++", "c", "h") and "fragment" not in info:
                    self.erreurs.append(f"{self.chemin} : bloc ```{lang} sans fichier derrière : mets le code dans "
                                        "Docs/exemples/ (compilé) ou marque-le ```cpp fragment")
                if lang in ("cpp", "c++", "c", "h"):
                    out.append(f'<div class="code"><div class="barre"><span class="fichier">C++</span><span class="droite-b">'
                               f'<span class="badge fragment">{u["fragment"]}</span></span></div><pre>{colorer_cpp(code)}</pre></div>')
                else:
                    lib = {"jenga": "Jenga (.jenga)", "bash": "Terminal", "texte": "", "text": ""}.get(lang, lang)
                    out.append(f'<div class="code"><div class="barre"><span class="fichier">{e(lib)}</span></div><pre>{e(code)}</pre></div>')
                i = j + 1
                continue
            if s.startswith("|"):
                fin_para()
                rangs = []
                while i < len(lignes) and lignes[i].strip().startswith("|"):
                    rangs.append([c.strip() for c in lignes[i].strip().strip("|").split("|")])
                    i += 1
                tete = rangs[0]
                corps = [r for r in rangs[1:] if not all(re.match(r"^:?-+:?$", c) for c in r)]
                out.append("<table><thead><tr>" + "".join(f"<th>{self.inline(c)}</th>" for c in tete) + "</tr></thead><tbody>"
                           + "".join("<tr>" + "".join(f"<td>{self.inline(c)}</td>" for c in r) + "</tr>" for r in corps)
                           + "</tbody></table>")
                continue
            if s.startswith(">"):
                fin_para()
                bloc = []
                while i < len(lignes) and lignes[i].strip().startswith(">"):
                    bloc.append(lignes[i].strip()[1:].strip())
                    i += 1
                genre = "note"
                titre = u["note"]
                if bloc and re.match(r"^\[!(attention|pourquoi|note)\]", bloc[0]):
                    g = re.match(r"^\[!(\w+)\]\s*(.*)$", bloc[0])
                    genre = g.group(1)
                    titre = g.group(2) or {"attention": u["attention"], "pourquoi": "Pourquoi" if self.langue == "fr" else "Why",
                                           "note": u["note"]}[genre]
                    bloc = bloc[1:]
                cls = {"attention": "note attention", "pourquoi": "pourquoi", "note": "note"}[genre]
                paras = " ".join(bloc)
                out.append(f'<div class="{cls}"><span class="nt">{e(titre)}</span>{self.inline(paras)}</div>')
                continue
            if re.match(r"^(-|\d+\.)\s+", s):
                fin_para()
                ordonnee = bool(re.match(r"^\d+\.", s))
                items = []
                while i < len(lignes) and re.match(r"^(-|\d+\.)\s+", lignes[i].strip()):
                    item = re.sub(r"^(-|\d+\.)\s+", "", lignes[i].strip())
                    i += 1
                    while i < len(lignes) and lignes[i].startswith("  ") and lignes[i].strip() and \
                            not re.match(r"^(-|\d+\.)\s+", lignes[i].strip()):
                        item += " " + lignes[i].strip()
                        i += 1
                    items.append(f"<li>{self.inline(item)}</li>")
                tag = "ol" if ordonnee else "ul"
                out.append(f"<{tag}>{''.join(items)}</{tag}>")
                continue
            if not s:
                fin_para()
                i += 1
                continue
            para.append(s)
            i += 1
        fin_para()
        return "\n".join(out)


# ---------------------------------------------------------------------------------------------------------------
# La référence : modules, en-têtes, types
# ---------------------------------------------------------------------------------------------------------------

def dossier_module(api: Dict[str, Any]) -> str:
    return "reference/" + api["_couche"].lower() + "/" + api["projet"].lower() + "/"


def page_entete(fichier: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", os.path.basename(fichier).lower()).strip("-")


def page_type(s: Dict[str, Any]) -> str:
    return re.sub(r"[^a-z0-9]+", "-", s["qualifie"].lower()).strip("-")


def lien_symbole(base: str, mod: str, api: Dict[str, Any], s: Dict[str, Any]) -> str:
    d = base + dossier_module(api)
    if s["nature"] in ("classe", "struct", "union", "enum") and not s.get("parent"):
        return d + page_type(s) + "/index.html"
    if s.get("parent"):
        par = next((p for p in api["symboles"] if p["id"] == s["parent"]), None)
        if par is not None:
            return d + page_type(par) + "/index.html#" + ancre_symbole(api, s)
    return d + page_entete(s["fichier"]) + "/index.html#" + ancre_symbole(api, s)


def ancre_symbole(api: Dict[str, Any], s: Dict[str, Any]) -> str:
    """Une ancre stable par symbole ; les surcharges sont numérotées dans l'ordre de l'en-tête."""
    base = ancre(s["nom"] or s["nature"])
    meme = [x for x in api["symboles"] if x.get("parent") == s.get("parent") and x["nom"] == s["nom"]
            and x["fichier"] == s["fichier"]]
    if len(meme) > 1:
        return base + "-" + str(meme.index(s) + 1)
    return base


def brief(site: Site, s: Dict[str, Any], langue: str, mod: str) -> Tuple[str, bool]:
    """La description courte dans la langue demandée. En anglais : la traduction si elle existe pour CE texte
    français (clé : identifiant + empreinte du français), sinon le français, marqué."""
    fr = ((s.get("doc") or {}).get("brief") or "").strip()
    if langue == "fr" or not fr:
        # une date en tête de description (« (19/10/2026) Écrire... ») est de l'histoire interne : elle ne s'affiche pas
        return re.sub(r"^\(\d\d/\d\d/20\d\d[^)]*\)\s*", "", fr), True
    tr = site.traductions.get(mod, {})
    en = tr.get(s["id"] + "|" + empreinte(fr))
    if en:
        return en, True
    site.manques_trad.setdefault(mod, set()).add(s["id"])
    return fr, False


def bloc_symbole(site: Site, s: Dict[str, Any], api: Dict[str, Any], langue: str, mod: str, chemin: str,
                 etiquette: str) -> str:
    u = UI[langue]
    d = s.get("doc") or {}
    a = ancre_symbole(api, s)
    b, ok = brief(site, s, langue, mod)
    attr_fr = "" if ok else ' lang="fr"'
    badges = [f'<span class="badge">{e(NATURES[langue][s["nature"]])}</span>']
    if s.get("modele"):
        badges.append(f'<span class="badge">{u["modele"]}</span>')
    if s.get("option"):
        badges.append(f'<span class="badge version">{u["option"]}</span>')
    if d.get("deprecie") is not None or s.get("deprecie_attr"):
        badges.append(f'<span class="badge deprec">{u["deprecie"]}</span>')
    corps = []
    if b:
        corps.append(f"<p{attr_fr}>{texte_inline(b)}</p>")
    if langue == "fr":
        for x in public(d.get("details", [])):
            corps.append(f"<p>{texte_inline(x)}</p>")
    ps = s.get("parametres") or []
    if ps:
        lignes = "".join(f"<tr><td><code>{e(p['nom'])}</code></td><td><code>{e(p['type'])}</code></td>"
                         f"<td>{texte_inline(d.get('params', {}).get(p['nom'], '')) if langue == 'fr' else ''}</td>"
                         f"<td>{('<code>' + e(p['defaut']) + '</code>') if p.get('defaut') else ''}</td></tr>" for p in ps)
        corps.append(f'<h4>{u["parametres"]}</h4><table><thead><tr><th>{u["nom"]}</th><th>{u["type"]}</th>'
                     f'<th>{u["role"]}</th><th>{u["defaut"]}</th></tr></thead><tbody>{lignes}</tbody></table>')
    if langue == "fr" and d.get("retour"):
        corps.append(f'<h4>{u["retour"]}</h4><p>{texte_inline(d["retour"])}</p>')
    if langue == "fr":
        for n in public(d.get("notes", [])):
            corps.append(f'<div class="note"><span class="nt">{u["note"]}</span>{texte_inline(n)}</div>')
        for w in public(d.get("avertissements", [])):
            corps.append(f'<div class="note attention"><span class="nt">{u["attention"]}</span>{texte_inline(w)}</div>')
    if s.get("valeurs"):
        vs = "".join(f"<tr><td><code>{e(v['nom'])}</code></td><td>{texte_inline(v.get('doc') or '') if langue == 'fr' else ''}</td></tr>"
                     for v in s["valeurs"])
        corps.append(f'<h4>{u["valeurs"]}</h4><table><tbody>{vs}</tbody></table>')
    for c in d.get("exemples", []):
        corps.append(f'<div class="code"><div class="barre"><span class="fichier">@code</span><span class="droite-b">'
                     f'<span class="badge fragment">{u["fragment"]}</span></span></div><pre>{colorer_cpp(c)}</pre></div>')
    src = f'{site.config["depot"]}/blob/{etiquette}/{api["source"]["racine"]}/{s["fichier"]}#L{s["ligne"]}'
    return (f'<section class="membre" id="{a}"><div class="tete"><h3>{e(s["nom"] or s["qualifie"])}</h3>'
            f'<a class="ancre" href="#{a}" aria-label="#">#</a><div class="badges">{"".join(badges)}</div></div>'
            f'<div class="sig">{colorer_cpp(s.get("signature", ""))}</div><div class="corps">{"".join(corps)}</div>'
            f'<div class="pied"><span>{u["source"]} : <a href="{e(src)}">{e(os.path.basename(s["fichier"]))}, '
            f'{u["ligne"]} {s["ligne"]}</a></span></div></section>')


def md_symbole(s: Dict[str, Any], langue: str, site: Site, mod: str) -> str:
    b, _ = brief(site, s, langue, mod)
    return f"### {s['nom'] or s['qualifie']}\n\n```cpp\n{s.get('signature', '')}\n```\n\n{tiret(b)}\n"


def tableau_resume(site: Site, syms: List[Dict[str, Any]], api: Dict[str, Any], langue: str, mod: str, chemin: str,
                   base: str, ici: bool) -> str:
    lignes = []
    for s in syms:
        b, ok = brief(site, s, langue, mod)
        cible = lien_symbole(base, mod, api, s)
        href = "#" + cible.split("#", 1)[1] if ici and "#" in cible else Site.rel(chemin, cible)
        lignes.append(f'<tr><td><a href="{e(href)}"><code>{e(s["nom"] or s["qualifie"])}</code></a></td>'
                      f'<td{"" if ok else " lang=\"fr\""}>{texte_inline(b)}</td></tr>')
    return f'<table class="membres"><tbody>{"".join(lignes)}</tbody></table>'


def arbre_reference(site: Site, chemin: str, base: str, projet: Dict[str, Any], mod_actif: str, entete_actif: str,
                    langue: str) -> str:
    out = [f'<nav class="arbre" aria-label="{UI[langue]["reference"]}">']
    for c in projet["couches"]:
        out.append(f'<div class="titre-arbre">{e(c["nom"])}</div>')
        for m in c["modules"]:
            cls = "couche-" + c["nom"][0].lower()
            if m in site.apis:
                api = site.apis[m]
                actif = ' class="actif"' if m == mod_actif and not entete_actif else ""
                out.append(f'<a{actif} href="{Site.rel(chemin, base + dossier_module(api) + "index.html")}"><span class="puce {cls}"></span>{e(m)}</a>')
                if m == mod_actif:
                    for f in api["fichiers"]:
                        pe = page_entete(f["chemin"])
                        act = " actif" if pe == entete_actif else ""
                        out.append(f'<a class="n3{act}" href="{Site.rel(chemin, base + dossier_module(api) + pe + "/index.html")}">{e(os.path.basename(f["chemin"]))}</a>')
            else:
                out.append(f'<span class="a-venir"><span class="puce {cls}"></span>{e(m)}</span>')
    out.append("</nav>")
    return "".join(out)


# --- diagrammes SVG (notre propre disposition : boîtes en rangées, flèches droites) -------------------------------

def svg_rangees(rangees: List[List[Tuple[str, str]]], titre: str, couleurs: Optional[Dict[str, str]] = None,
                aretes: Optional[List[Tuple[str, str]]] = None, liens: Optional[Dict[str, str]] = None) -> str:
    """Des boîtes disposées en rangées (la première en haut) ; les arêtes vont de la boîte `a` vers la boîte `b`.
    Les couleurs passent par les variables de la feuille (thèmes clair et sombre)."""
    larg_b, haut_b, ecart_x, ecart_y = 150, 34, 18, 46
    # une rangée trop longue se replie (six boîtes au plus par ligne)
    pliees: List[List[Tuple[str, str]]] = []
    for r in rangees:
        for i in range(0, len(r), 6):
            pliees.append(r[i:i + 6])
    rangees = pliees
    larg = max(len(r) for r in rangees) * (larg_b + ecart_x) + ecart_x
    haut = len(rangees) * (haut_b + ecart_y) + ecart_y - 20
    pos: Dict[str, Tuple[float, float]] = {}
    for iy, r in enumerate(rangees):
        decal = (larg - (len(r) * (larg_b + ecart_x) - ecart_x)) / 2
        for ix, (cle, _) in enumerate(r):
            pos[cle] = (decal + ix * (larg_b + ecart_x), 14 + iy * (haut_b + ecart_y))
    morceaux = [f'<svg class="diagramme" viewBox="0 0 {larg:.0f} {haut:.0f}" width="{larg:.0f}" role="img" '
                f'aria-label="{e(titre)}" xmlns="http://www.w3.org/2000/svg"><title>{e(titre)}</title>'
                '<defs><marker id="fl" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" '
                'orient="auto-start-reverse"><path d="M0 0L10 5L0 10z" class="fleche"/></marker></defs>']
    for a, b in aretes or []:
        if a in pos and b in pos:
            (xa, ya), (xb, yb) = pos[a], pos[b]
            x1, y1 = xa + larg_b / 2, ya + (haut_b if yb > ya else 0)
            x2, y2 = xb + larg_b / 2, yb + (0 if yb > ya else haut_b)
            if abs(ya - yb) < 1:
                y1 = y2 = ya + haut_b / 2
                x1, x2 = (xa + larg_b, xb) if xb > xa else (xa, xb + larg_b)
            morceaux.append(f'<line x1="{x1:.0f}" y1="{y1:.0f}" x2="{x2:.0f}" y2="{y2:.0f}" class="arete" marker-end="url(#fl)"/>')
    for r in rangees:
        for cle, lib in r:
            x, y = pos[cle]
            coul = (couleurs or {}).get(cle, "")
            style = f' style="stroke:{coul}"' if coul else ""
            boite = (f'<rect x="{x:.0f}" y="{y:.0f}" width="{larg_b}" height="{haut_b}" rx="6" class="boite"{style}/>'
                     f'<text x="{x + larg_b / 2:.0f}" y="{y + haut_b / 2 + 4:.0f}" text-anchor="middle" class="lib">{e(lib)}</text>')
            if liens and cle in liens:
                boite = f'<a href="{e(liens[cle])}">{boite}</a>'
            morceaux.append(boite)
    morceaux.append("</svg>")
    return "".join(morceaux)


def nom_simple(t: str) -> str:
    t = re.sub(r"<.*>", "", t)
    return t.split("::")[-1].strip()


def hierarchie(site: Site, s: Dict[str, Any]) -> Tuple[List[str], List[Dict[str, Any]]]:
    bases = [b["type"] for b in s.get("bases", [])]
    derivees = []
    for api in site.apis.values():
        for x in api["symboles"]:
            for b in x.get("bases", []):
                if nom_simple(b["type"]) == s["nom"] and x is not s:
                    derivees.append(x)
    return bases, derivees


def generer_reference(site: Site, projet: Dict[str, Any], version: Dict[str, Any], langue: str) -> None:
    u = UI[langue]
    base = f"{langue}/{projet['id']}/{version['version']}/"
    etiquette = version["etiquette"]
    pid, vv = projet["id"], version["version"]
    # index de la référence
    chemin = base + "reference/index.html"
    tete = (f'<div class="ariane"><a href="{Site.rel(chemin, base + "index.html")}">{e(projet["nom"])}</a><span class="s">/</span>{u["reference"]}</div>'
            f"<h1>{u['reference']}</h1><p class=\"chapeau\">{e(espace(site, projet, 'reference')['r' + langue])}</p>")
    md = [f"# {u['reference']}", ""]
    couches = []
    for c in projet["couches"]:
        mods = []
        for m in c["modules"]:
            if m in site.apis:
                mods.append(f'<a class="puce-mod" href="{Site.rel(chemin, base + dossier_module(site.apis[m]) + "index.html")}">{e(m)}</a>')
                md.append(f"- [{m}]({dossier_module(site.apis[m])[len('reference/'):]}index.md)")
            else:
                mods.append(f'<span class="puce-mod vide" title="{u["a_venir"]}">{e(m)}</span>')
        couches.append(f'<div class="couche" style="--c:{c["couleur"]}"><div class="nom-c">{e(c["nom"])}</div><div class="mods">{"".join(mods)}</div></div>')
    corps_idx = tete + f'<div class="couches">{"".join(couches)}</div>' + pied(site, langue, chemin, etiquette=etiquette)
    site.ecrire(chemin, gabarit(site, chemin, langue, f"{u['reference']} · {projet['nom']} {vv}", corps_idx,
                                projet, version, "reference",
                                arbre=arbre_reference(site, chemin, base, projet, "", "", langue)), "\n".join(md) + "\n")

    for mod, api in site.apis.items():
        info = projet["documentes"][mod]
        d = base + dossier_module(api)
        # --- page du module ---
        chemin = d + "index.html"
        cv = couverture(api)
        g = site.graphe.get("projets", {})
        dep = g.get(mod, {}).get("depend_de", [])
        tous_mods = {m for c in projet["couches"] for m in c["modules"]}
        par = sorted(n for n, p in g.items() if mod in p.get("depend_de", []) and n in tous_mods)
        lien_mod = {m: Site.rel(chemin, base + dossier_module(site.apis[m]) + "index.html") for m in site.apis}
        rangees = []
        if dep:
            rangees.append([(x, x) for x in dep])
        rangees.append([(mod, mod)])
        if par:
            rangees.append([(x, x) for x in par])
        aretes = [(mod, x) for x in dep] + [(x, mod) for x in par]
        svg = svg_rangees(rangees, f"{u['dependances']} : {mod}", aretes=aretes, liens=lien_mod)
        lignes_f = []
        for f in api["fichiers"]:
            n = sum(1 for s in api["symboles"] if s["fichier"] == f["chemin"] and not s.get("parent"))
            lignes_f.append(f'<tr><td><a href="{Site.rel(chemin, d + page_entete(f["chemin"]) + "/index.html")}"><code>'
                            f'{e(os.path.basename(f["chemin"]))}</code></a></td><td>{texte_inline(f.get("brief")) if langue == "fr" else ""}</td>'
                            f'<td>{n}</td></tr>')
        inc = f'{mod}/{mod}.h' if any(os.path.basename(f["chemin"]) == f"{mod}.h" for f in api["fichiers"]) else ""
        corps = f"""<div class="ariane"><a href="{Site.rel(chemin, base + 'reference/index.html')}">{u['reference']}</a><span class="s">/</span>{e(info['couche'])}<span class="s">/</span>{e(mod)}</div>
<h1>{e(mod)}</h1>
<div class="badges"><span class="badge">{u['module']}</span><span class="badge version">{e(etiquette)}</span></div>
<p class="chapeau">{e(info[langue])}</p>
<div class="utiliser" id="utiliser">
  <div class="k">{u['lier']}</div><div><code>nkentseudependson(["{e(mod)}"])</code></div>
  {f'<div class="k">{u["inclure"]}</div><div><code>#include "{e(inc)}"</code></div>' if inc else ''}
  <div class="k">{u['couverture']}</div><div><span class="jauge"><span class="barre-j"><i style="width:{cv['pourcent']:.0f}%"></i></span> {cv['documentes']} / {cv['total']} ({str(cv['pourcent']).replace('.', ',') if langue == 'fr' else cv['pourcent']} %)</span></div>
</div>
<h2 id="dependances">{u['dependances']}</h2>
<p>{u['depend_de']} : {', '.join(f'<code>{e(x)}</code>' for x in dep) or u['aucune']}. {u['utilise_par']} : {', '.join(f'<code>{e(x)}</code>' for x in par) or u['aucune']}.</p>
<figure class="figure-svg">{svg}</figure>
<h2 id="entetes">{u['entetes']}</h2>
<table class="membres"><tbody>{''.join(lignes_f)}</tbody></table>
{pied(site, langue, chemin, etiquette=etiquette)}"""
        md = (f"# {mod}\n\n{info[langue]}\n\n- {u['lier']} : `nkentseudependson([\"{mod}\"])`\n- {u['couverture']} : "
              f"{cv['documentes']} / {cv['total']}\n- {u['depend_de']} : {', '.join(dep) or u['aucune']}\n- "
              f"{u['utilise_par']} : {', '.join(par) or u['aucune']}\n\n## {u['entetes']}\n\n"
              + "\n".join(f"- [{os.path.basename(f['chemin'])}]({page_entete(f['chemin'])}/index.md)" for f in api["fichiers"]) + "\n")
        site.ecrire(chemin, gabarit(site, chemin, langue, f"{mod} · {u['reference']} {projet['nom']} {vv}", corps, projet,
                                    version, "reference", toc=[("utiliser", u["pour_utiliser"], 2), ("dependances", u["dependances"], 2),
                                                               ("entetes", u["entetes"], 2)],
                                    arbre=arbre_reference(site, chemin, base, projet, mod, "", langue)), md)
        site.indexer(langue, pid, vv, "R", mod, f"{info['couche']} › {mod}", Site.rel(base + "x", chemin), info[langue])

        # --- une page par en-tête ---
        for f in api["fichiers"]:
            pe = page_entete(f["chemin"])
            chemin = d + pe + "/index.html"
            syms = [s for s in api["symboles"] if s["fichier"] == f["chemin"] and not s.get("parent")]
            types = [s for s in syms if s["nature"] in ("classe", "struct", "union", "enum")]
            inclure = f["chemin"].split("src/", 1)[-1]
            espaces_n = sorted({s["espace"] for s in syms if s["espace"]})
            parties = [f'<div class="ariane"><a href="{Site.rel(chemin, base + "reference/index.html")}">{u["reference"]}</a><span class="s">/</span>'
                       f'{e(info["couche"])}<span class="s">/</span><a href="{Site.rel(chemin, d + "index.html")}">{e(mod)}</a></div>',
                       f"<h1>{e(os.path.basename(f['chemin']))}</h1>",
                       f'<p class="chapeau">{texte_inline(f.get("brief")) if langue == "fr" else ""}</p>',
                       f'<div class="utiliser" id="utiliser"><div class="k">{u["inclure"]}</div><div><code>#include "{e(inclure)}"</code></div>'
                       + (f'<div class="k">{u["espace_noms"]}</div><div>{", ".join("<code>" + e(x) + "</code>" for x in espaces_n)}</div>' if espaces_n else "")
                       + f'<div class="k">{u["lier"]}</div><div><code>nkentseudependson(["{e(mod)}"])</code></div></div>']
            toc = [("utiliser", u["pour_utiliser"], 2)]
            md = [f"# {os.path.basename(f['chemin'])}", "", f"`#include \"{inclure}\"`", ""]
            if types:
                parties.append(f'<h2 id="types">{u["types"]}</h2>' + tableau_resume(site, types, api, langue, mod, chemin, base, False))
                toc.append(("types", u["types"], 2))
                md += [f"## {u['types']}", ""] + [f"- [{t['qualifie']}](../{page_type(t)}/index.md)" for t in types] + [""]
            for nat, cle in (("fonction", "fonctions"), ("alias", "alias"), ("variable", "variables"), ("macro", "macros")):
                g2 = [s for s in syms if s["nature"] == nat]
                if not g2:
                    continue
                parties.append(f'<h2 id="{cle}">{u[cle]} <span class="compte">· {len(g2)}</span></h2>'
                               + tableau_resume(site, g2, api, langue, mod, chemin, base, True))
                toc.append((cle, u[cle], 2))
                md += [f"## {u[cle]}", ""]
                for s in g2:
                    md.append(md_symbole(s, langue, site, mod))
                    site.indexer(langue, pid, vv, "R", s["qualifie"], f"{mod} › {os.path.basename(f['chemin'])}",
                                 Site.rel(base + "x", lien_symbole(base, mod, api, s)), (brief(site, s, langue, mod)[0])[:80])
            details = [bloc_symbole(site, s, api, langue, mod, chemin, etiquette) for s in syms
                       if s["nature"] not in ("classe", "struct", "union", "enum")]
            if details:
                parties.append(f'<h2 id="details">{u["details"]}</h2>' + "".join(details))
                toc.append(("details", u["details"], 2))
            parties.append(pied(site, langue, chemin, etiquette=etiquette))
            site.ecrire(chemin, gabarit(site, chemin, langue, f"{os.path.basename(f['chemin'])} · {mod} {vv}", "".join(parties),
                                        projet, version, "reference", toc=toc,
                                        arbre=arbre_reference(site, chemin, base, projet, mod, pe, langue)), "\n".join(md) + "\n")
            site.indexer(langue, pid, vv, "R", os.path.basename(f["chemin"]), f"{mod}", Site.rel(base + "x", chemin),
                         f.get("brief", "") if langue == "fr" else "")

            # --- une page par type ---
            for t in types:
                chemin_t = d + page_type(t) + "/index.html"
                membres = [s for s in api["symboles"] if s.get("parent") == t["id"]]
                b, ok = brief(site, t, langue, mod)
                bases, derivees = hierarchie(site, t)
                h = [f'<h2 id="hierarchie">{u["hierarchie"]}</h2>',
                     f'<p>{u["bases"]} : {", ".join("<code>" + e(x) + "</code>" for x in bases) or u["aucune"]}. '
                     f'{u["derivees"]} : {", ".join("<code>" + e(x["qualifie"]) + "</code>" for x in derivees) or u["aucune"]}.</p>']
                rangees = []
                if bases:
                    rangees.append([("b:" + x, nom_simple(x)) for x in bases])
                rangees.append([("t", t["nom"])])
                if derivees:
                    rangees.append([("d:" + x["id"], x["nom"]) for x in derivees])
                aretes = [("t", "b:" + x) for x in bases] + [("d:" + x["id"], "t") for x in derivees]
                h.append(f'<figure class="figure-svg" data-diagramme="classes">{svg_rangees(rangees, u["hierarchie"] + " : " + t["nom"], aretes=aretes)}</figure>')
                groupes: Dict[str, List[Dict[str, Any]]] = {}
                for m in membres:
                    groupes.setdefault((m.get("doc") or {}).get("groupe") or u["membres"], []).append(m)
                partie_m = [f'<h2 id="membres">{u["membres"]} <span class="compte">· {len(membres)}</span></h2>']
                for gname, ms in groupes.items():
                    if len(groupes) > 1:
                        partie_m.append(f'<h3 id="g-{ancre(gname)}">{e(gname)} <span class="compte">· {len(ms)}</span></h3>')
                    partie_m.append(tableau_resume(site, ms, api, langue, mod, chemin_t, base, True))
                corps_t = (f'<div class="ariane"><a href="{Site.rel(chemin_t, base + "reference/index.html")}">{u["reference"]}</a><span class="s">/</span>'
                           f'{e(info["couche"])}<span class="s">/</span><a href="{Site.rel(chemin_t, d + "index.html")}">{e(mod)}</a><span class="s">/</span>'
                           f'<a href="{Site.rel(chemin_t, d + pe + "/index.html")}">{e(os.path.basename(f["chemin"]))}</a></div>'
                           f'<h1>{e(t["nom"])}</h1><div class="badges"><span class="badge">{e(NATURES[langue][t["nature"]])}</span>'
                           + (f'<span class="badge">{u["modele"]}</span>' if t.get("modele") else "") + "</div>"
                           f'<p class="chapeau"{"" if ok else " lang=\"fr\""}>{texte_inline(b)}</p>'
                           f'<div class="utiliser" id="utiliser"><div class="k">{u["inclure"]}</div><div><code>#include "{e(inclure)}"</code></div>'
                           f'<div class="k">{u["espace_noms"]}</div><div><code>{e(t["espace"] or "::")}</code></div>'
                           f'<div class="k">{u["lier"]}</div><div><code>nkentseudependson(["{e(mod)}"])</code></div>'
                           f'<div class="k">C++</div><div><code>{e(t.get("signature", ""))}</code></div></div>'
                           + "".join(h) + ("".join(partie_m) if membres else "")
                           + (f'<h2 id="details">{u["details"]}</h2>' + "".join(bloc_symbole(site, m, api, langue, mod, chemin_t, etiquette) for m in membres) if membres else "")
                           + (bloc_symbole(site, t, api, langue, mod, chemin_t, etiquette) if t["nature"] == "enum" else "")
                           + pied(site, langue, chemin_t, etiquette=etiquette))
                toc_t = [("utiliser", u["pour_utiliser"], 2), ("hierarchie", u["hierarchie"], 2)]
                if membres:
                    toc_t += [("membres", u["membres"], 2), ("details", u["details"], 2)]
                md_t = (f"# {t['qualifie']}\n\n{tiret(b)}\n\n`#include \"{inclure}\"`\n\n## {u['hierarchie']}\n\n"
                        f"- {u['bases']} : {', '.join(bases) or u['aucune']}\n- {u['derivees']} : "
                        f"{', '.join(x['qualifie'] for x in derivees) or u['aucune']}\n\n## {u['membres']}\n\n"
                        + "\n".join(md_symbole(m, langue, site, mod) for m in membres))
                site.ecrire(chemin_t, gabarit(site, chemin_t, langue, f"{t['nom']} · {mod} {vv}", corps_t, projet, version,
                                              "reference", toc=toc_t, arbre=arbre_reference(site, chemin_t, base, projet, mod, pe, langue)), md_t)
                site.indexer(langue, pid, vv, "R", t["qualifie"], f"{mod} › {os.path.basename(f['chemin'])}",
                             Site.rel(base + "x", chemin_t), b[:80])
                for m in membres:
                    site.indexer(langue, pid, vv, "R", t["nom"] + "::" + (m["nom"] or ""), f"{mod} › {t['nom']}",
                                 Site.rel(base + "x", lien_symbole(base, mod, api, m)), brief(site, m, langue, mod)[0][:80])


def couverture(api: Dict[str, Any]) -> Dict[str, Any]:
    """La jauge se recalcule ici, sur les symboles AFFICHÉS (espaces de détail retirés) : un nombre stocké pourrait
    être périmé, un @brief retiré doit faire baisser la jauge."""
    syms = [s for s in api["symboles"] if s["nature"] != "champ"]
    ok = sum(1 for s in syms if (s.get("doc") or {}).get("brief"))
    return {"documentes": ok, "total": len(syms), "pourcent": round(100.0 * ok / len(syms), 1) if syms else 100.0}


def espace(site: Site, projet: Dict[str, Any], ident: str) -> Dict[str, str]:
    return next(x for x in site.config["genres"][projet.get("genre", "moteur")]["espaces"] if x["id"] == ident)


# ---------------------------------------------------------------------------------------------------------------
# Les pages écrites (Apprendre, Comprendre, Faire)
# ---------------------------------------------------------------------------------------------------------------

def generer_pages_ecrites(site: Site, projet: Dict[str, Any], version: Dict[str, Any], langue: str) -> Dict[str, List[Dict[str, str]]]:
    u = UI[langue]
    base = f"{langue}/{projet['id']}/{version['version']}/"
    src = DOCS / "site" / langue / projet["id"]
    par_espace: Dict[str, List[Dict[str, str]]] = {}
    if not src.is_dir():
        return par_espace
    pages = []
    for p in sorted(src.rglob("*.md")):
        rel = p.relative_to(src).with_suffix("").as_posix()
        meta, texte = lire_page(p)
        esp = rel.split("/", 1)[0]
        pages.append((rel, meta, texte, esp, p))
    for rel, meta, texte, esp, p in pages:
        chemin = base + rel + "/index.html"
        r = Rendu(site, chemin, langue, base, projet["id"], version["version"])
        texte = filtrer_versions(texte, version["version"])
        corps_html = r.rendre(texte)
        site.erreurs.extend(r.erreurs)
        e_info = espace(site, projet, esp)
        ariane = (f'<div class="ariane"><a href="{Site.rel(chemin, base + "index.html")}">{e(projet["nom"])}</a><span class="s">/</span>'
                  f'<a href="{Site.rel(chemin, base + esp + "/index.html")}">{e(e_info[langue])}</a></div>')
        resume = meta.get("resume", "")
        etapes = ""
        corps = (ariane + f"<h1>{e(meta.get('titre', rel))}</h1>"
                 + (f'<p class="chapeau">{r.inline(resume)}</p>' if resume else "") + etapes + corps_html
                 + pied(site, langue, chemin, etiquette=version["etiquette"]))
        md = f"# {meta.get('titre', rel)}\n\n{resume}\n\n" + texte_md(texte, site)
        site.ecrire(chemin, gabarit(site, chemin, langue, f"{meta.get('titre', rel)} · {projet['nom']} {version['version']}",
                                    corps, projet, version, esp, toc=r.toc or None,
                                    arbre=arbre_ecrit(site, chemin, base, pages, esp, rel, langue, projet)), md)
        k = ESPACE_REPERTOIRE.get(esp, "M")
        site.indexer(langue, projet["id"], version["version"], k, meta.get("titre", rel), e_info[langue],
                     Site.rel(base + "x", chemin), meta.get("cles", "") + " " + resume)
        par_espace.setdefault(esp, []).append({"titre": meta.get("titre", rel), "resume": resume, "chemin": chemin,
                                               "ordre": meta.get("ordre", "50")})
        site._liens_a_verifier = getattr(site, "_liens_a_verifier", []) + [(chemin, x) for x in r.liens_internes]
    return par_espace


def filtrer_versions(texte: str, version: str) -> str:
    """Les lignes entre {{si-version:a,b}} et {{fin-si}} ne restent que dans les versions nommées (le « problème
    connu » d'une étiquette, l'exemple d'une API corrigée après elle)."""
    out, garder = [], True
    for l in texte.split("\n"):
        m = re.match(r"^\{\{si-version:([^}]+)\}\}\s*$", l.strip())
        if m:
            garder = version in [x.strip() for x in m.group(1).split(",")]
            continue
        if l.strip() == "{{fin-si}}":
            garder = True
            continue
        if garder:
            out.append(l)
    return "\n".join(out)


def texte_md(texte: str, site: Site) -> str:
    """La jumelle .md d'une page écrite : les directives d'exemple sont remplacées par le code réel et sa sortie."""
    def rempl(m):
        ref, ext = m.group(2).strip(), m.group(3)
        ex = site.exemples.get(ref)
        if not ex or ex["statut"] != "ok":
            return ""
        code = ex["extraits"].get(ext, "") if ext else ex["code"]
        s = f"```cpp\n{code.rstrip()}\n```\n"
        if not ext:
            s += f"\nSortie :\n\n```text\n{ex['sortie'].rstrip()}\n```\n"
        return s
    return re.sub(r"^\{\{(exemple|extrait):([^}#]+)(?:#([^}]+))?\}\}$", rempl, texte, flags=re.M)


def arbre_ecrit(site: Site, chemin: str, base: str, pages, esp: str, rel_actif: str, langue: str,
                projet: Dict[str, Any]) -> str:
    out = [f'<nav class="arbre" aria-label="{e(espace(site, projet, esp)[langue])}"><div class="titre-arbre">{e(espace(site, projet, esp)[langue])}</div>']
    for rel, meta, _, e2, _ in sorted(pages, key=lambda x: (x[1].get("ordre", "50"), x[0])):
        if e2 != esp:
            continue
        cls = ' class="actif"' if rel == rel_actif else ""
        out.append(f'<a{cls} href="{Site.rel(chemin, base + rel + "/index.html")}">{e(meta.get("titre", rel))}</a>')
    out.append("</nav>")
    return "".join(out)


def generer_index_espaces(site: Site, projet: Dict[str, Any], version: Dict[str, Any], langue: str,
                          par_espace: Dict[str, List[Dict[str, str]]]) -> None:
    u = UI[langue]
    base = f"{langue}/{projet['id']}/{version['version']}/"
    for esp in site.config["genres"][projet.get("genre", "moteur")]["espaces"]:
        if esp["id"] == "reference":
            continue
        chemin = base + esp["id"] + "/index.html"
        items = sorted(par_espace.get(esp["id"], []), key=lambda x: (x["ordre"], x["titre"]))
        liste = "".join(f'<a class="carte" href="{Site.rel(chemin, x["chemin"])}"><h3>{e(x["titre"])}</h3><p>{e(x["resume"])}</p></a>'
                        for x in items)
        corps = (f'<div class="ariane"><a href="{Site.rel(chemin, base + "index.html")}">{e(projet["nom"])}</a><span class="s">/</span>{e(esp[langue])}</div>'
                 f'<h1>{e(esp[langue])}</h1><p class="chapeau">{e(esp["r" + langue])}</p><div class="liste-cartes">{liste}</div>'
                 + pied(site, langue, chemin, etiquette=version["etiquette"]))
        md = f"# {esp[langue]}\n\n{esp['r' + langue]}\n\n" + "\n".join(
            f"- [{x['titre']}]({os.path.relpath(x['chemin'], os.path.dirname(chemin)).replace(os.sep, '/')[:-5]}.md) : {x['resume']}" for x in items) + "\n"
        site.ecrire(chemin, gabarit(site, chemin, langue, f"{esp[langue]} · {projet['nom']} {version['version']}", corps,
                                    projet, version, esp["id"], classe_main="page-espace"), md)


# ---------------------------------------------------------------------------------------------------------------
# L'accueil d'un projet, et le portail
# ---------------------------------------------------------------------------------------------------------------

def graphe_couches(site: Site, projet: Dict[str, Any], langue: str, chemin: str, base: str) -> str:
    """Le graphe des couches : une arête de A vers B dès qu'un module de A dépend d'un module de B (lu dans les .jenga)."""
    couche_de = {m: c["nom"] for c in projet["couches"] for m in c["modules"]}
    aretes = set()
    g = site.graphe.get("projets", {})
    for m, c in couche_de.items():
        for dep in g.get(m, {}).get("depend_de", []):
            if dep in couche_de and couche_de[dep] != c:
                aretes.add((c, couche_de[dep]))
    rangees = [[(c["nom"], c["nom"]) for c in projet["couches"]]]   # une rangée : la couche du dessus à gauche
    coul = {c["nom"]: c["couleur"] for c in projet["couches"]}
    # seules les arêtes vers la couche voisine sont dessinées ; les sauts sont dits dans le texte (lisibilité)
    ordre = [c["nom"] for c in projet["couches"]]
    voisines = sorted(a for a in aretes if ordre.index(a[0]) - ordre.index(a[1]) == 1)
    sauts = sorted(a for a in aretes if ordre.index(a[0]) - ordre.index(a[1]) > 1)
    inverses = sorted(a for a in aretes if ordre.index(a[0]) < ordre.index(a[1]))
    # une flèche va de la couche qui dépend vers celle dont elle dépend (même sens que les pages de module)
    svg = svg_rangees(rangees, UI[langue]["graphe_couches"], couleurs=coul, aretes=voisines)
    txt_sauts = ", ".join(f"{a} → {b}" for a, b in sauts)
    txt_inv = ", ".join(f"{a} → {b}" for a, b in inverses)
    lib_s = "Dépendances directes vers une couche plus basse" if langue == "fr" else "Direct dependencies on a lower layer"
    lib_i = "À REVOIR : une couche dépend d'une couche du dessous" if langue == "fr" else "TO CHECK: a layer depends on a layer below it"
    return (f'<figure class="figure-svg" data-diagramme="couches" data-aretes="{e(";".join(a + ">" + b for a, b in sorted(aretes)))}">{svg}'
            f'<figcaption>{lib_s} : {e(txt_sauts) or "-"}.' + (f' <strong>{lib_i} : {e(txt_inv)}</strong>' if inverses else "")
            + "</figcaption></figure>")


def generer_accueil_projet(site: Site, projet: Dict[str, Any], version: Dict[str, Any], langue: str,
                           par_espace: Dict[str, List[Dict[str, str]]], exemples: Dict[str, Any]) -> None:
    u = UI[langue]
    base = f"{langue}/{projet['id']}/{version['version']}/"
    chemin = base + "index.html"
    cartes = []
    for esp in site.config["genres"][projet.get("genre", "moteur")]["espaces"]:
        items = sorted(par_espace.get(esp["id"], []), key=lambda x: (x["ordre"], x["titre"]))[:3]
        if esp["id"] == "reference":
            items = [{"titre": m, "chemin": base + dossier_module(site.apis[m]) + "index.html"} for m in site.apis]
        lis = "".join(f'<li><a href="{Site.rel(chemin, x["chemin"])}">{e(x["titre"])}</a></li>' for x in items)
        cartes.append(f'<div class="carte"><a class="carte-tete" href="{Site.rel(chemin, base + esp["id"] + "/index.html")}">'
                      f'<div class="ic">{ICONES.get(esp["id"], "")}</div><h3>{e(esp[langue])}</h3></a><p>{e(esp["r" + langue])}</p><ul>{lis}</ul></div>')
    couches = []
    for c in projet["couches"]:
        mods = "".join((f'<a class="puce-mod" href="{Site.rel(chemin, base + dossier_module(site.apis[m]) + "index.html")}">{e(m)}</a>'
                        if m in site.apis else f'<span class="puce-mod vide" title="{u["a_venir"]}">{e(m)}</span>') for m in c["modules"])
        nb = sum(1 for m in c["modules"] if m in site.apis)
        couches.append(f'<div class="couche" style="--c:{c["couleur"]}"><div class="nom-c">{e(c["nom"])}<small>{len(c["modules"])} modules · '
                       f'{nb} {"documenté" + ("s" if nb > 1 else "") if langue == "fr" else "documented"}</small></div><div class="mods">{mods}</div></div>')
    ok = sum(1 for x in exemples.values() if x["statut"] == "ok")
    tot = len(exemples)
    doc_ok = sum(couverture(a)["documentes"] for a in site.apis.values())
    doc_tot = sum(couverture(a)["total"] for a in site.apis.values())
    briefs = sum(1 for a in site.apis.values() for s in a["symboles"] if (s.get("doc") or {}).get("brief"))
    trad = sum(1 for mod, a in site.apis.items() for s in a["symboles"]
               if (s.get("doc") or {}).get("brief") and site.traductions.get(mod, {}).get(s["id"] + "|" + empreinte(s["doc"]["brief"].strip())))
    def jauge(n, t, lib):
        p = (100.0 * n / t) if t else 100.0
        coul = "" if p >= 90 else ";background:var(--warn)"
        return f'<div><span class="jauge"><span class="barre-j"><i style="width:{p:.0f}%{coul}"></i></span> {lib} : {n} / {t}</span></div>'
    sante = jauge(ok, tot, u["exemples_ok"]) + jauge(doc_ok, doc_tot, u["couverture"]) + jauge(trad, briefs, u["traduit"])
    titre_h = ('<span class="mot">Nkentseu</span>, <span>de la mémoire au jeu</span>' if langue == "fr"
               else '<span class="mot">Nkentseu</span>, <span>from memory to game</span>')
    sous = ("Chaque couche du moteur, de Foundation aux applications. Chaque exemple de ce site est compilé et exécuté à chaque construction de la documentation."
            if langue == "fr" else "Every layer of the engine, from Foundation to the applications. Every example on this site is compiled and run each time the docs are built.")
    corps = f"""<section class="heros">
  <div class="sur">{"Moteur et outils de Rihen" if langue == "fr" else "Rihen engine and tools"}</div>
  <h1>{titre_h}</h1>
  <p>{sous}</p>
  <button class="grande-recherche" data-recherche>{ICONES["recherche"]}<span class="txt">{"Une fonction, une macro, une notion : NkByteSwap32, environnement..." if langue == "fr" else "A function, a macro, a concept: NkByteSwap32, environment..."}</span><span class="kbds"><kbd>Ctrl</kbd><kbd>K</kbd></span></button>
  <div class="vers">{u["version"]} {e(libelle_version(version))}</div>
</section>
<div class="quatre">{"".join(cartes)}</div>
<div class="section-titre"><h2>{u["couche_titre"]}</h2><span>{u["couche_sous"]}</span></div>
<div class="couches">{"".join(couches)}</div>
<div class="deux">
  <div><div class="section-titre"><h2>{u["graphe_couches"]}</h2></div>{graphe_couches(site, projet, langue, chemin, base)}</div>
  <div><div class="section-titre"><h2>{u["sante"]}</h2><span>{e(libelle_version(version))}</span></div><div class="liste-nouv">{sante}</div></div>
</div>
{pied(site, langue, chemin, etiquette=version["etiquette"])}"""
    md = (f"# {projet['nom']} {version['version']}\n\n{projet[langue]}\n\n"
          + "\n".join(f"- [{esp[langue]}]({esp['id']}/index.md) : {esp['r' + langue]}" for esp in site.config['genres'][projet.get('genre', 'moteur')]['espaces']) + "\n")
    site.ecrire(chemin, gabarit(site, chemin, langue, f"{projet['nom']} {version['version']} · {u['doc']}", corps, projet, version), md)
    site.indexer(langue, projet["id"], version["version"], "M", projet["nom"], u["accueil"], "index.html", projet[langue])


def generer_portail(site: Site, langue: str) -> None:
    u = UI[langue]
    for chemin in ([f"{langue}/index.html"] + (["index.html"] if langue == "fr" else [])):
        blocs = []
        principal = next(p for p in site.config["projets"] if p.get("principal"))
        v = next(x for x in principal["versions"] if x.get("derniere"))
        cible = f"{langue}/{principal['id']}/{v['version']}/index.html"
        fam0 = next(f for f in site.config["familles"] if f["id"] == principal["famille"])
        blocs.append(f'<div class="section-titre"><h2 id="{fam0["id"]}">{e(fam0[langue])}</h2></div>')
        blocs.append(f'<a class="projet-principal" href="{Site.rel(chemin, cible)}"><span class="sur">{u["principal"]}</span>'
                     f'<b>{e(principal["nom"])}</b><span>{e(principal[langue])}</span><span class="aller">{u["ouvrir"]} · {e(v["etiquette"].lstrip("v"))} →</span></a>')
        for f in site.config["familles"]:
            projets = [p for p in site.config["projets"] if p["famille"] == f["id"] and not p.get("principal")]
            if not projets:
                continue
            cartes = []
            for p in projets:
                if p["etat"] == "actif":
                    vp = next(x for x in p["versions"] if x.get("derniere"))
                    cartes.append(f'<a class="produit" href="{Site.rel(chemin, f"{langue}/{p["id"]}/{vp["version"]}/index.html")}"><b>{e(p["nom"])}</b><small>{e(p[langue])}</small></a>')
                else:
                    nom = p.get("nom_" + langue, p["nom"]) if langue != "fr" else p["nom"]
                    cartes.append(f'<div class="produit a-venir" aria-disabled="true"><span class="v">{u["a_venir"]}</span><b>{e(nom)}</b><small>{e(p[langue])}</small></div>')
            blocs.append(f'<div class="section-titre"><h2 id="{f["id"]}">{e(f[langue])}</h2></div><div class="produits">{"".join(cartes)}</div>')
        corps = (f'<section class="heros"><div class="sur">Rihen</div><h1>{u["portail_titre"]}</h1><p>{u["portail_sous"]}</p></section>'
                 + "".join(blocs) + pied(site, langue, chemin))
        md = (f"# {u['portail_titre']}\n\n{u['portail_sous']}\n\n- [{principal['nom']}]({os.path.relpath(cible, os.path.dirname(chemin) or '.').replace(os.sep, '/')[:-5]}.md)\n"
              + "\n".join(f"- {p.get('nom_' + langue, p['nom']) if langue != 'fr' else p['nom']} ({u['a_venir']}) : {p[langue]}" for p in site.config["projets"] if not p.get("principal")) + "\n")
        site.ecrire(chemin, gabarit(site, chemin, langue, u["portail_titre"], corps), md)


# ---------------------------------------------------------------------------------------------------------------
# Index de recherche, alias « dernière », et l'assemblage
# ---------------------------------------------------------------------------------------------------------------

def ecrire_index(site: Site) -> None:
    for (langue, pid, vv), entrees in site.index.items():
        js = ("// Index de recherche généré par Docs/outils/generer_site.py (" + langue + ", " + pid + " " + vv + ").\n"
              "window.NK_INDEX = " + json.dumps(entrees, ensure_ascii=False, separators=(",", ":")) + ";\n")
        p = site.sortie / langue / pid / vv / "recherche.js"
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(js, encoding="utf-8")


def ecrire_alias(site: Site) -> None:
    """« derniere » (fr) et « latest » (en) mènent à la dernière version publiée : une page d'accueil qui redirige,
    et la page 404 de GitHub Pages qui redirige tout chemin profond (sans copier le site)."""
    regles = {}
    for p in site.config["projets"]:
        if p["etat"] != "actif":
            continue
        v = next(x for x in p["versions"] if x.get("derniere"))
        for langue, alias in (("fr", "derniere"), ("en", "latest")):
            chemin = f"{langue}/{p['id']}/{alias}/index.html"
            cible = f"../{v['version']}/index.html"
            site.ecrire(chemin, f'<!DOCTYPE html><html lang="{langue}"><head><meta charset="utf-8"><meta http-equiv="refresh" content="0; url={cible}">'
                                f'<title>{e(p["nom"])}</title></head><body><a href="{cible}">{e(p["nom"])} {e(v["version"])}</a></body></html>\n')
            regles[f"/{langue}/{p['id']}/{alias}/"] = f"/{langue}/{p['id']}/{v['version']}/"
    js = json.dumps(regles)
    site.ecrire("404.html", f"""<!DOCTYPE html><html lang="fr"><head><meta charset="utf-8"><title>Page introuvable · Rihen</title>
<script>(function(){{var r={js};for(var k in r){{var i=location.pathname.indexOf(k);if(i>=0){{location.replace(location.pathname.slice(0,i)+r[k]+location.pathname.slice(i+k.length)+location.hash);return;}}}}}})();</script>
</head><body><p>Page introuvable. <a href="/">Retour au portail</a>.</p></body></html>
""")


def copier_theme(site: Site) -> None:
    dst = site.sortie / "assets"
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(DOCS / "theme", dst)


def charger(site: Site) -> None:
    c = site.construction
    for p in site.config["projets"]:
        for mod, info in (p.get("documentes") or {}).items():
            # une référence PAR VERSION : api/<version>/<Module>/api.json (l'étiquette, ou main pour « dev »)
            for v in p.get("versions", []):
                f = c / "api" / v["version"] / info["api"] / "api.json"
                api = json.loads(f.read_text(encoding="utf-8"))
                api["_couche"] = info["couche"]
                # le même filtre que le site public : les espaces de noms de détail ne sont pas l'API
                api["symboles"] = [s for s in api["symboles"]
                                   if not re.search(r"(^|::)(\w*_)?(detail|details|internal|impl)(::|$)", s.get("espace", ""))]
                site.apis_par_version.setdefault(v["version"], {})[mod] = api
            # la référence de main sert à la garde « version » (un exemple est construit contre main)
            fm = c / "api" / "main" / info["api"] / "api.json"
            if fm.exists():
                site.apis_main[mod] = json.loads(fm.read_text(encoding="utf-8"))
            tf = DOCS / "traductions" / "en" / f"{mod}.json"
            site.traductions[mod] = json.loads(tf.read_text(encoding="utf-8")).get("entrees", {}) if tf.exists() else {}
    g = c / "graphe.json"
    site.graphe = json.loads(g.read_text(encoding="utf-8")) if g.exists() else {}
    ex = c / "exemples.json"
    if ex.exists():
        for x in json.loads(ex.read_text(encoding="utf-8"))["exemples"]:
            site.exemples[f"{x['couche']}/{x['module']}/{x['nom']}"] = x


def verifier_liens_internes(site: Site) -> None:
    for depuis, cible in getattr(site, "_liens_a_verifier", []):
        if cible not in site.pages:
            site.erreurs.append(f"{depuis} : le lien @/ mène à {cible}, qui n'existe pas")


def main() -> int:
    # la console Windows est en cp1252 (et -I ignore PYTHONIOENCODING) : les messages sortent en UTF-8
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description="Génère le site de documentation de Rihen (statique, hors ligne).")
    ap.add_argument("--construction", required=True, help="Dossier contenant api/<Module>/api.json, graphe.json, exemples.json")
    ap.add_argument("--sortie", required=True, help="Dossier du site à écrire (vidé d'abord)")
    ap.add_argument("--docs", help="Autre dossier Docs/ (pour les contre-épreuves du banc), défaut : celui de ce script")
    a = ap.parse_args()
    global DOCS
    if a.docs:
        DOCS = Path(a.docs)
    config = json.loads((DOCS / "site" / "site.json").read_text(encoding="utf-8"))
    sortie = Path(a.sortie)
    if sortie.exists():
        shutil.rmtree(sortie)
    sortie.mkdir(parents=True)
    site = Site(sortie, config, Path(a.construction))
    charger(site)
    copier_theme(site)
    for langue in config["langues"]:
        generer_portail(site, langue)
        for p in config["projets"]:
            if p["etat"] != "actif":
                continue
            for v in p["versions"]:
                site.apis = site.apis_par_version.get(v["version"], {})
                par_espace = generer_pages_ecrites(site, p, v, langue)
                generer_reference(site, p, v, langue)
                generer_index_espaces(site, p, v, langue, par_espace)
                generer_accueil_projet(site, p, v, langue, par_espace, site.exemples)
    verifier_liens_internes(site)
    ecrire_index(site)
    ecrire_alias(site)
    cname = DOCS / "publication" / "CNAME"
    if cname.exists():
        shutil.copy(cname, sortie / "CNAME")
    (sortie / ".nojekyll").write_text("", encoding="utf-8")
    (sortie / "construction.json").write_text(json.dumps({
        "pages": len(site.pages), "exemples": {k: v["statut"] for k, v in site.exemples.items()},
        "anglais_manquant": {k: len(v) for k, v in site.manques_trad.items()},
        "paragraphes_internes_ecartes": len(set(ECARTES)), "erreurs": site.erreurs},
        ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"{len(site.pages)} pages écrites dans {sortie}")
    for x in site.erreurs:
        print("ERREUR " + x)
    return 1 if site.erreurs else 0


if __name__ == "__main__":
    sys.exit(main())
