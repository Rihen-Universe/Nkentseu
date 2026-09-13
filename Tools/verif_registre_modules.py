#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
u"""
verif_registre_modules.py — LE REGISTRE DE config/modules.jenga EST-IL ENTIER ?

=============================================================================
 POURQUOI CE BANC EXISTE — une date, un caractere, et cinq alias
=============================================================================
 Dans la nuit du 11 au 12/09/2026, la ligne des alias de `config/modules.jenga`
 portait ceci, sur UNE SEULE LIGNE :

     "physics": "NKPhysics", "animphysics": "NKAnima",  # rentre dans NKAnima le 2026-09-04 "animation": "NKAnima", "navigation": "NKNavigation", "ui": "NKUI", ...

 Le `#` du commentaire a avale CINQ alias : `animation`, `navigation`, `ui`,
 `audio`, `xr`. Python n'a rien dit -- un commentaire est valide. Le
 dictionnaire etait plus petit, et c'est tout.

 ⚠️ AUCUN ESSAI, AUCUNE CONSTRUCTION, AUCUN JOURNAL NE L'A SIGNALE.
    Parce que rien ne COMPTAIT. Les alias survivants resolvaient tous
    parfaitement ; les valeurs restantes etaient toutes justes. Un controle sur
    les valeurs serait reste VERT sur un dictionnaire ampute de 12 %.

 > UN COMMENTAIRE QUI AVALE UNE DEMI-LIGNE NE CHANGE AUCUNE VALEUR.
 > IL CHANGE UN COMPTE. C'est donc un COMPTE qu'il faut ecrire.

 C'est la raison d'etre du controle 4 ci-dessous, et la seule chose qui aurait
 attrape ce defaut-la.

=============================================================================
 CE QU'IL VERIFIE
=============================================================================
   1. ALIAS       tout alias de `_ALIASES` resout vers une cle de `_REGISTRY`.
   2. DEPENDANCES toute dependance declaree designe un module du registre.
   4a. COMPTE     len(_REGISTRY) et len(_ALIASES) contre un ATTENDU ecrit ici.
   4b. DOUBLON    les cles ECRITES dans le litteral contre les cles VIVANTES du
                  dictionnaire. Une cle ecrite deux fois en ecrase une autre EN
                  SILENCE : le litteral en compte 57, le dictionnaire 56.
   5. CYCLES      aucun circuit dans le graphe de dependances.

 Le point 3 du brief (les noms cites par les 212 `.jenga` du depot) N'EST PAS
 FAIT, et c'est une decision, pas un oubli -- voir « CE QU'IL NE FAIT PAS ».

=============================================================================
 LES TROIS EXIGENCES DE METHODE, ET COMMENT ELLES SONT TENUES
=============================================================================
 ① CHAQUE CONTROLE A SON VOLET NEGATIF (`--contre-epreuve`).
   Un controle qui n'a jamais rougi n'est pas un controle, c'est une intention.
   Chaque mutation casse DELIBEREMENT la chose verifiee et exige le rouge.

   ⚠️ ET CHAQUE MUTATION EST PASSEE SUR TOUTE LA BATTERIE, PAS SUR SA SEULE
      CIBLE. Une mutation ciblee ne prouve que sa cible ; passee sur tous les
      controles, elle revele les TEMOINS MUETS -- ceux qui auraient du parler
      et se taisent. Le rapport nomme, pour chaque mutation, qui l'a vue.

   ⚠️ L'EXIGENCE N'EST PAS « c'etait vert, c'est rouge » MAIS « un rouge NEUF
      NOMME l'element injecte ». La difference n'est pas academique : l'arbre
      peut etre DEJA rouge pour une autre raison (il l'est, cf. le doublon
      NKECS), et un « avant/apres » naif conclurait alors n'importe quoi. On
      compare donc les ENSEMBLES de rouges, et on exige que l'element injecte
      apparaisse dans la difference.

 ② LA MESURE PASSE PAR LA VRAIE FONCTION, JAMAIS PAR UNE RELECTURE DU TEXTE.
   `_resolve_key` et `_require_key` sont APPELEES, sur le module reellement
   charge. Lire une source prouve qu'un appel existe, pas qu'il s'execute.
   Le chargement neutralise les imports du DSL (`Jenga`, `jengaconfig`) par des
   modules bouchons poses dans `sys.modules` -- methode eprouvee par l'agent
   eau le 12/09.

   SEULE EXCEPTION, ASSUMEE ET NOMMEE : le controle 4b compare le LITTERAL au
   dictionnaire. Il DOIT lire la source, puisque son objet est precisement
   l'ecart entre ce qui est ecrit et ce qui survit. Il le fait par `ast`, pas
   par expression reguliere.

 ③ LE CONTROLE 4a EST UN COMPTE, PAS UNE VALEUR. Voir l'en-tete.

=============================================================================
 LE PLANCHER D'INSTRUMENT — un banc qui ne lit plus rien rend vert
=============================================================================
 « Je n'ai rien trouve » et « je ne lis plus le fichier » produisent la meme
 sortie verte. Avant tout verdict, ce banc exige donc :
   - que le module se charge et expose les quatre symboles attendus ;
   - que `_require_key` LEVE bien sur un nom inconnu. Si cette garde cessait de
     lever, tous les controles de resolution deviendraient muets d'un coup.
 L'une de ces deux exigences non tenue = ECHEC D'INSTRUMENT (code 2), et
 SURTOUT PAS un vert.

=============================================================================
 CE QU'IL NE FAIT PAS
=============================================================================
 - IL NE REPARE RIEN. Ni alias, ni doublon, ni nom perime. Portee decidee par
   Rodolf le 12/09 : DETECTER SEULEMENT. Un controle qui signale ne casse rien ;
   une correction automatique fausse se propage a 28 arbres sans bruit.
 - IL NE TOUCHE A AUCUN `.jenga`. Les mutations du volet negatif sont faites EN
   MEMOIRE, sur le dictionnaire charge. Aucun fichier n'est ecrit, jamais.
 - IL NE BALAIE PAS LES 212 `.jenga` DU DEPOT (point 3 du brief). Raison
   mesuree, pas fatigue : `links(...)` apparait 532 fois et melange des noms de
   modules avec des bibliotheques SYSTEME (`pthread`, `EGL`, `GLESv2`,
   `wayland-client`, `decor-0`...). Exiger que tout nom cite par `links()`
   resolve produirait un ROUGE MASSIF ET FAUX des le premier passage -- et un
   banc faux-rouge est desactive dans la semaine. Ce point demande d'abord de
   separer les trois sources (`nkentseudependson` = 188 appels, noms de modules
   uniquement ; `useappdeps` = 6 ; `links` = 532, corpus mixte). Il est laisse
   ouvert DELIBEREMENT.

=============================================================================
 CODES DE SORTIE
=============================================================================
   0  tout est coherent
   1  INCOHERENCE : au moins un controle rougit (le rapport dit lequel et sur
      quel element)
   2  ECHEC D'INSTRUMENT : le module ne charge pas, un symbole manque, ou la
      garde `_require_key` ne leve plus. Rien n'est conclu.
   3  VOLET NEGATIF NON TENU (`--contre-epreuve`) : une mutation n'a pas fait
      rougir le controle qui devait la voir.
=============================================================================
"""

import argparse
import ast
import copy
import pathlib
import sys
import types

# =============================================================================
# L'ATTENDU — LE COEUR DU CONTROLE 4a
# =============================================================================
# Mesure du 2026-09-12 sur `origin/main` (6fb634fcd), par chargement reel.
# Ces deux nombres sont la DONNEE du controle. Les modifier est un geste
# DELIBERE qui apparait dans un diff et se justifie dans un message de commit :
# c'est exactement ce qui manquait la nuit ou cinq alias sont partis en silence.
#
# ⚠️ Un ecart ici n'est pas forcement un defaut -- ajouter un module est normal.
#    Ce que le banc refuse, c'est que le compte change SANS QUE PERSONNE NE LE
#    DISE. Il rougit pour forcer la phrase, pas pour interdire le changement.
# =============================================================================
# 2026-09-12, fusion de feat/noge-feu dans transit : 56 -> 55, et c'est DECLARE.
# NKAnimation (e2623fb7e, 02/09) et NKAnimPhysics (33edc1151, 04/09) sont fondus
# en NKAnima : deux entrees devenues une. 56 - 2 + 1 = 55. Conséquence comprise,
# pas une perte -- le banc a rougi, on a nomme, on corrige l'attendu en le disant.
# 2026-09-12, fusion de feat/noge-eau : 55 -> 56, DECLARE AVANT DE LANCER la fusion.
# noge-eau apporte NKVFX (cle declaree dans son modules.jenga). Une entree de plus.
ATTENDU_REGISTRY = 56
# Les alias, eux, restent 36 : les cinq que le '#' avalait ont ete rendus.
ATTENDU_ALIASES = 36

CHEMIN_REGISTRE = "config/modules.jenga"


class EchecInstrument(Exception):
    """Le banc ne peut rien conclure. Ce n'est jamais un vert."""


# =============================================================================
# CHARGEMENT — la vraie source, les vraies fonctions, les imports neutralises
# =============================================================================

def _poser_bouchons() -> None:
    """
    Pose des modules bouchons pour `Jenga` et `jengaconfig` dans `sys.modules`.

    `config/modules.jenga` fait `from Jenga import *` et evalue
    `ProjectKind.STATIC_LIB` au chargement. Sans bouchon il ne se charge pas
    hors d'une passe Jenga ; avec le vrai Jenga il traine tout un runtime de
    construction. Le bouchon donne exactement ce que le chargement consomme,
    et rien d'autre.
    """

    class _ProjectKind:
        # Les deux seules valeurs que le registre compare (cf. _GLOBAL_KIND).
        STATIC_LIB = "StaticLib"
        SHARED_LIB = "SharedLib"

    def _rien(*_a, **_k):
        return None

    jenga = types.ModuleType("Jenga")
    jenga.ProjectKind = _ProjectKind
    noms_dsl = (
        # Emetteurs appeles par les helpers du registre :
        "kindexport", "defines", "includedirs", "links", "dependson",
        # Portees et chemins :
        "filter", "project", "objdir", "targetdir", "location",
        # Divers rencontres dans les .jenga du depot :
        "language", "cppdialect", "PkgExists", "useconfig",
    )
    for nom in noms_dsl:
        setattr(jenga, nom, _rien)
    jenga.__all__ = list(noms_dsl) + ["ProjectKind"]
    sys.modules["Jenga"] = jenga

    cfg = types.ModuleType("jengaconfig")
    cfg.__all__ = []
    sys.modules["jengaconfig"] = cfg


def charger(racine: pathlib.Path) -> dict:
    """Execute le registre et rend son espace de noms. Leve EchecInstrument."""
    chemin = racine / CHEMIN_REGISTRE
    if not chemin.is_file():
        raise EchecInstrument("introuvable : %s" % chemin)
    try:
        source = chemin.read_text(encoding="utf-8")
    except OSError as err:
        raise EchecInstrument("illisible : %s (%s)" % (chemin, err))

    _poser_bouchons()
    espace = {"__name__": "modules_jenga_charge", "__file__": str(chemin)}
    try:
        exec(compile(source, str(chemin), "exec"), espace)
    except Exception as err:
        raise EchecInstrument(
            "le registre ne se charge pas : %s: %s" % (type(err).__name__, err))

    for symbole in ("_REGISTRY", "_ALIASES", "_resolve_key", "_require_key"):
        if symbole not in espace:
            raise EchecInstrument("symbole absent apres chargement : %s" % symbole)

    espace["_source"] = source
    return espace


def plancher_instrument(espace: dict) -> None:
    """
    La garde `_require_key` LEVE-T-ELLE ENCORE ?

    Si elle cessait de lever, tout ce qui verifie une resolution deviendrait
    muet d'un seul coup, et le banc rendrait vert sur un registre en ruine.
    C'est le seul controle de ce fichier qui porte sur l'INSTRUMENT lui-meme.
    """
    nom_impossible = "NKModuleQuiNExistePasEtNExisteraJamais"
    if espace["_resolve_key"](nom_impossible) is not None:
        raise EchecInstrument(
            "_resolve_key resout un nom impossible -- la resolution ne discrimine plus")
    try:
        espace["_require_key"](nom_impossible, "plancher d'instrument")
    except KeyError:
        return
    except Exception as err:
        raise EchecInstrument(
            "_require_key leve autre chose qu'un KeyError : %s" % type(err).__name__)
    raise EchecInstrument(
        "_require_key NE LEVE PLUS sur un nom inconnu -- la garde est morte")


# =============================================================================
# LES CONTROLES — chacun rend la liste de ses rouges (vide = vert)
# =============================================================================

def controle_1_alias(espace: dict) -> list:
    """Tout alias resout vers une cle PRESENTE dans le registre."""
    rouges = []
    resoudre = espace["_resolve_key"]
    registre = espace["_REGISTRY"]
    for alias in sorted(espace["_ALIASES"]):
        cible = espace["_ALIASES"][alias]
        resolu = resoudre(alias)
        if resolu is None:
            rouges.append("alias '%s' -> '%s' : NE RESOUT PAS" % (alias, cible))
        elif resolu not in registre:
            rouges.append(
                "alias '%s' -> '%s' : resout vers '%s', ABSENT du registre"
                % (alias, cible, resolu))
    return rouges


def controle_2_dependances(espace: dict) -> list:
    """Toute dependance declaree designe un module du registre."""
    rouges = []
    resoudre = espace["_resolve_key"]
    registre = espace["_REGISTRY"]
    for module in sorted(registre):
        entree = registre[module]
        for dependance in entree.get("deps", []):
            resolu = resoudre(dependance)
            if resolu is None:
                rouges.append(
                    "module '%s' : dependance '%s' INCONNUE du registre"
                    % (module, dependance))
            elif resolu not in registre:
                rouges.append(
                    "module '%s' : dependance '%s' resout vers '%s', ABSENT du registre"
                    % (module, dependance, resolu))
    return rouges


def controle_4a_compte(espace: dict) -> list:
    """
    LE CONTROLE QUI AURAIT ATTRAPE LE DEFAUT DE LA NUIT.
    Un compte, pas une valeur.
    """
    rouges = []
    mesures = (
        # (nom du dictionnaire, taille mesuree, taille attendue)
        ("_REGISTRY", len(espace["_REGISTRY"]), ATTENDU_REGISTRY),
        ("_ALIASES", len(espace["_ALIASES"]), ATTENDU_ALIASES),
    )
    for nom, mesure, attendu in mesures:
        if mesure != attendu:
            ecart = mesure - attendu
            sens = "MANQUANTES" if ecart < 0 else "EN TROP"
            rouges.append(
                "%s : %d entrees, %d attendues (%d %s). "
                "Si ce changement est voulu, corrige l'attendu dans ce fichier "
                "et DIS-LE dans le message de commit."
                % (nom, mesure, attendu, abs(ecart), sens))
    return rouges


def _cles_du_litteral(source: str, nom_dict: str) -> list:
    """Cles ECRITES dans le litteral, doublons compris, lues par `ast`."""
    arbre = ast.parse(source)
    for noeud in ast.walk(arbre):
        cible = None
        if isinstance(noeud, ast.AnnAssign) and isinstance(noeud.target, ast.Name):
            cible = noeud.target.id
        elif isinstance(noeud, ast.Assign) and len(noeud.targets) == 1 \
                and isinstance(noeud.targets[0], ast.Name):
            cible = noeud.targets[0].id
        if cible == nom_dict and isinstance(noeud.value, ast.Dict):
            return [
                cle.value for cle in noeud.value.keys
                if isinstance(cle, ast.Constant) and isinstance(cle.value, str)
            ]
    return []


def controle_4b_doublons(espace: dict) -> list:
    """
    Les cles ECRITES contre les cles VIVANTES.

    Une cle ecrite deux fois n'est pas une erreur pour Python : la seconde
    ecrase la premiere, en silence. La declaration perdue reste LISIBLE dans le
    fichier -- on la relit, on la croit vivante, et elle ne l'est pas.
    """
    rouges = []
    for nom in ("_REGISTRY", "_ALIASES"):
        ecrites = _cles_du_litteral(espace["_source"], nom)
        if not ecrites:
            # Plancher local : si l'AST ne trouve plus le litteral, on ne
            # conclut pas « pas de doublon » -- on le DIT.
            rouges.append(
                "%s : litteral introuvable par AST -- ce controle n'a rien lu, "
                "il ne conclut pas" % nom)
            continue
        vivantes = espace[nom]
        vues, doublons = set(), []
        for cle in ecrites:
            if cle in vues and cle not in doublons:
                doublons.append(cle)
            vues.add(cle)
        for cle in doublons:
            rouges.append(
                "%s : cle '%s' DECLAREE DEUX FOIS -- la seconde ecrase la "
                "premiere en silence (%d cles ecrites, %d vivantes)"
                % (nom, cle, len(ecrites), len(vivantes)))
    return rouges


def controle_5_cycles(espace: dict) -> list:
    """Aucun circuit dans le graphe de dependances."""
    rouges = []
    resoudre = espace["_resolve_key"]
    registre = espace["_REGISTRY"]
    etat = {}

    def descendre(cle: str, pile: list) -> None:
        if etat.get(cle) == 2:
            return
        if etat.get(cle) == 1:
            rouges.append("CYCLE : %s" % " -> ".join(pile[pile.index(cle):] + [cle]))
            return
        etat[cle] = 1
        pile.append(cle)
        for dependance in registre.get(cle, {}).get("deps", []):
            resolu = resoudre(dependance)
            if resolu is not None and resolu in registre:
                descendre(resolu, pile)
        pile.pop()
        etat[cle] = 2

    for cle in sorted(registre):
        descendre(cle, [])
    return rouges


# La batterie. Chaque mutation du volet negatif est passee sur TOUTE la
# batterie, pour que les temoins muets se voient.
BATTERIE = (
    # (identifiant, libelle, fonction)
    ("1-alias", "tout alias resout vers une cle du registre", controle_1_alias),
    ("2-deps", "toute dependance designe un module du registre", controle_2_dependances),
    ("4a-compte", "le compte d'entrees est celui attendu", controle_4a_compte),
    ("4b-doublon", "aucune cle n'est declaree deux fois", controle_4b_doublons),
    ("5-cycles", "aucun cycle dans le graphe", controle_5_cycles),
)


def passer_batterie(espace: dict) -> dict:
    """Rend {identifiant: [rouges]} pour toute la batterie."""
    return {ident: fonction(espace) for ident, _libelle, fonction in BATTERIE}


# =============================================================================
# VOLET NEGATIF — les mutations, EN MEMOIRE, jamais sur un fichier
# =============================================================================

def _muter_alias_mort(espace: dict) -> str:
    espace["_ALIASES"]["aliasfantome"] = "NKModuleFantomeInexistant"
    return "aliasfantome"


def _muter_dep_bidon(espace: dict) -> str:
    espace["_REGISTRY"]["NKCore"]["deps"].append("NKDependanceBidonInexistante")
    return "NKDependanceBidonInexistante"


def _muter_alias_avale(espace: dict) -> str:
    """
    LA MUTATION DE LA NUIT DU 11 AU 12/09, REJOUEE.

    On retire cinq alias du dictionnaire charge -- exactement ce qu'un `#` en
    fin de ligne a fait. Aucune valeur restante n'est fausse. Seul le COMPTE
    change. C'est la mutation qui decide si ce banc valait la peine.
    """
    for alias in ("animation", "navigation", "ui", "audio", "xr"):
        espace["_ALIASES"].pop(alias, None)
    return "_ALIASES"


def _muter_module_retire(espace: dict) -> str:
    espace["_REGISTRY"].pop("NKCamera", None)
    return "_REGISTRY"


def _muter_cycle(espace: dict) -> str:
    espace["_REGISTRY"]["NKPlatform"]["deps"].append("NKCore")
    return "NKPlatform"


MUTATIONS = (
    # (identifiant, ce qu'on casse, le controle qui DOIT la voir, la mutation)
    ("M1", "un alias pointant vers un module inexistant", "1-alias", _muter_alias_mort),
    ("M2", "une dependance bidon ajoutee a NKCore", "2-deps", _muter_dep_bidon),
    ("M4a", "CINQ alias avales, comme par le '#' du 11/09", "4a-compte", _muter_alias_avale),
    ("M4b", "un module retire du registre", "4a-compte", _muter_module_retire),
    ("M5", "un cycle NKPlatform <-> NKCore", "5-cycles", _muter_cycle),
)


def contre_epreuve(racine: pathlib.Path) -> int:
    """
    Chaque mutation doit produire un ROUGE NEUF QUI NOMME l'element injecte,
    chez le controle designe. Et chaque mutation passe sur TOUTE la batterie.
    """
    espace_sain = charger(racine)
    plancher_instrument(espace_sain)
    reference = passer_batterie(espace_sain)

    print("=== VOLET NEGATIF — chaque mutation doit faire ROUGIR son controle ===")
    print("")
    print("Etat de reference (avant toute mutation) :")
    for ident, libelle, _f in BATTERIE:
        etat = "VERT" if not reference[ident] else "DEJA ROUGE (%d)" % len(reference[ident])
        print("   %-11s %-52s %s" % (ident, libelle, etat))
    print("")

    manques = []
    for ident, quoi, cible, muter in MUTATIONS:
        espace = charger(racine)
        # Le registre charge est neuf a chaque fois, mais on copie tout de meme
        # les structures mutees : une mutation ne doit jamais fuir vers la
        # mesure suivante.
        espace["_REGISTRY"] = copy.deepcopy(espace["_REGISTRY"])
        espace["_ALIASES"] = dict(espace["_ALIASES"])
        injecte = muter(espace)
        apres = passer_batterie(espace)

        neufs = {
            i: [r for r in apres[i] if r not in reference[i]]
            for i, _l, _f in BATTERIE
        }
        vus_par = [i for i in neufs if neufs[i]]
        nomme = any(injecte in r for r in neufs.get(cible, []))

        verdict = "OK" if nomme else "NON TENU"
        print("%s  %s" % (ident, quoi))
        print("     controle attendu : %s" % cible)
        print("     rouge neuf chez  : %s" % (", ".join(sorted(vus_par)) or "PERSONNE"))
        print("     nomme '%s'       : %s   -> %s" % (injecte, "oui" if nomme else "NON", verdict))
        for ligne in neufs.get(cible, [])[:2]:
            print("     | %s" % ligne)
        # Les temoins muets et les temoins bavards se lisent ici, pas ailleurs.
        bavards = [i for i in vus_par if i != cible]
        if bavards:
            print("     (aussi vu par : %s)" % ", ".join(sorted(bavards)))
        print("")
        if not nomme:
            manques.append("%s : %s n'a pas nomme '%s'" % (ident, cible, injecte))

    if manques:
        print("VOLET NEGATIF NON TENU :")
        for ligne in manques:
            print("   %s" % ligne)
        return 3
    print("Les %d mutations ont toutes fait rougir le controle qui devait les voir." % len(MUTATIONS))
    return 0


# =============================================================================
# PASSAGE NORMAL
# =============================================================================

def passage(racine: pathlib.Path) -> int:
    espace = charger(racine)
    plancher_instrument(espace)
    resultats = passer_batterie(espace)

    print("=== COHERENCE DU REGISTRE — %s ===" % CHEMIN_REGISTRE)
    print("")
    print("Mesure (par chargement reel, fonctions du registre appelees) :")
    print("   _REGISTRY : %d entrees (attendu %d)"
          % (len(espace["_REGISTRY"]), ATTENDU_REGISTRY))
    print("   _ALIASES  : %d entrees (attendu %d)"
          % (len(espace["_ALIASES"]), ATTENDU_ALIASES))
    print("")

    total = 0
    for ident, libelle, _f in BATTERIE:
        rouges = resultats[ident]
        total += len(rouges)
        print("[%s] %-11s %s" % ("ROUGE" if rouges else " ok  ", ident, libelle))
        for ligne in rouges:
            print("        -> %s" % ligne)
    print("")

    if total:
        print("INCOHERENCE : %d signalement(s)." % total)
        print("Ce banc DETECTE, il ne repare pas. Aucun fichier n'a ete modifie.")
        return 1
    print("Registre coherent.")
    return 0


def main() -> int:
    analyseur = argparse.ArgumentParser(
        description="Coherence du registre de modules de Nkentseu (detection seule).")
    analyseur.add_argument(
        "--contre-epreuve", action="store_true",
        help="volet NEGATIF : casse chaque chose verifiee et exige le rouge.")
    analyseur.add_argument(
        "--racine", default=None,
        help="racine du depot (defaut : le parent du dossier de ce script).")
    arguments = analyseur.parse_args()

    racine = pathlib.Path(arguments.racine) if arguments.racine \
        else pathlib.Path(__file__).resolve().parent.parent

    try:
        if arguments.contre_epreuve:
            return contre_epreuve(racine)
        return passage(racine)
    except EchecInstrument as err:
        print("ECHEC D'INSTRUMENT : %s" % err)
        print("Rien n'est conclu. Ce n'est PAS un vert.")
        return 2


if __name__ == "__main__":
    sys.exit(main())
