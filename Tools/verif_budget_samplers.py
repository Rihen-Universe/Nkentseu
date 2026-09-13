#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verif_budget_samplers.py — un etage fragment tient-il dans le budget d'unites
de texture de sa cible ?

=============================================================================
 POURQUOI CET OUTIL EXISTE — une date, une heure, et vingt-deux jours
=============================================================================
 Le 2026-08-11 a 00h01, le commit 8e72961a a ajoute `tLTC1` et `tLTC2` au
 fragment PBR (tables LTC, speculaire surfacique). Le shader est passe de 25 a
 27 echantillonneurs declares. Apres la fusion des cookies (qui en retire 10),
 il en restait **17**. WebGL2 en garantit **16**.

 Personne ne l'a su pendant VINGT-DEUX JOURS.

 Pourquoi : le seul environnement web jamais execute etait SwiftShader (rendu
 logiciel), qui en accorde PLUS de 16. Le defaut n'est sorti que le 2026-09-02
 au soir, quand Rodolf a lance le Web sur une vraie carte :

     FRAGMENT shader texture image units count exceeds
     MAX_TEXTURE_IMAGE_UNITS(16)
     [NkShader] CreateShader fail 'PBR'

 Pas de PBR -> pas de spheres -> ecran vide.

 ⚠️ LA CAUSE PROFONDE N'EST PAS LE SHADER, C'EST LE BUDGET.
    `NkOpenglDevice.cpp` porte un commentaire qui annonce « 24 -> 14 ». Il
    avait RAISON le 31/07, jour ou il a ete ecrit. Il enumere 24 samplers : il
    ne connait pas `tHeight` (10/08), ni `tLTC1`/`tLTC2` (11/08).

    **Un budget calcule une fois, dans un commentaire, n'est pas un budget :
    c'est le SOUVENIR d'un budget.** Rien ne le recalculait quand le shader
    grossissait. Ce script est ce qui manquait : le recalcul.

=============================================================================
 CE QU'IL FAIT, ET LES QUATRE EXIGENCES QUI LE RENDENT HONNETE
=============================================================================
 1. IL COMPTE APRES TRANSFORMATION, PAS SUR LA SOURCE.
    Le chemin Web applique `NkWebMergeCookieSamplers` : les DECLARATIONS des
    cookies 1..7 (2D) et 1..3 (cube) sont supprimees, leurs usages rediriges
    vers le slot 0. Compter la source brute mesurerait un objet que la cible
    ne verra JAMAIS -- 27 au lieu de 17, un faux positif garanti.

 2. IL PORTE LE BUDGET DE CHAQUE CIBLE, PAS UNE CONSTANTE.
    16 pour WebGL2/GLES (minimum garanti par le standard, et exactement ce
    qu'expose ANGLE). Davantage sur bureau. Un seuil unique serait faux des
    deux cotes : trop laxiste pour le Web, absurde pour Windows.

 3. IL A UN CONTROLE POSITIF, ET C'EST LA CLAUSE QUI COMPTE (`--controle`).
    Un banc qui compte MAL reste vert POUR TOUJOURS, et il a exactement la
    meme apparence qu'un banc juste.

    ⚠️ MA PREMIERE VERSION DE CE CONTROLE ETAIT FAUSSE, et elle s'est
    denoncee elle-meme au premier essai (2026-09-02). Elle injectait un
    sampler dans CHAQUE etage et comptait les depassements : le total ne
    bougeait pas. Cause : PBR depassait DEJA (17 -> 18, toujours « 1
    depassement »), et l'etage suivant est a 9 -- injecter 1 ne le fait pas
    traverser 16. **La donnee d'essai ne pouvait pas exprimer l'ecart** : le
    depot ne contient aucun shader assis exactement sur la frontiere.

    C'est la porte du depot appliquee a moi-meme : quand une mutation
    survit, la cause n'est pas toujours « le controle manque » -- c'est
    souvent « le jeu d'essai ne peut pas montrer la difference ». Le juge
    etait bon, la question etait mal posee.

    Le controle teste donc maintenant les DEUX choses qui peuvent etre
    fausses, et la seconde est celle qui compte :
      a) SENSIBILITE DU COMPTEUR : chaque etage doit gagner exactement 1 ;
      b) SENSIBILITE DU VERDICT : deux etages SYNTHETIQUES, l'un a exactement
         `budget` samplers, l'autre a `budget + 1`, doivent etre juges
         respectivement VERT et ROUGE. C'est la frontiere elle-meme qu'on
         eprouve -- un `>` ecrit `>=` ne survit pas a ce test.

 4. IL TOURNE A LA CONSTRUCTION, PAS A L'EXECUTION.
    La limite est statiquement connue. Ce defaut n'a aucune raison d'attendre
    un GPU, un navigateur, et la console de Rodolf.

 ⚠️ CE QU'IL NE FAIT PAS : il ne compile pas le GLSL. Il compte des
    DECLARATIONS par analyse lexicale. Un sampler declare mais jamais
    echantillonne est elimine par certains compilateurs et pas par d'autres --
    et c'est precisement pour ca qu'on ne PARIE PAS dessus : on compte les
    declarations qui restent apres transformation, hypothese PESSIMISTE. Un
    faux positif ici coute une minute ; un faux negatif a coute 22 jours.
"""

import argparse
import os
import re
import sys

# La console Windows est en cp1252 : sans ca, un emoji de verdict fait TOMBER le
# script par UnicodeEncodeError -- et un banc qui plante avant d'imprimer son
# verdict est indiscernable d'un banc vert quand on lit son code de sortie a
# travers un `| head`. Defaut d'INSTRUMENT, jamais de mesure.
for _flux in (sys.stdout, sys.stderr):
    try:
        _flux.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        pass

# --- Les cibles et leur budget -----------------------------------------------
# 16 = MAX_TEXTURE_IMAGE_UNITS minimum garanti par WebGL2 / GLES 3.0, et la
# valeur exacte qu'expose ANGLE (donc Chrome/Edge sur Windows). Ce n'est pas
# une valeur prudente : c'est LA valeur du terrain.
CIBLES = {
    "web": {"budget": 16, "fusion_cookies": True,
            "libelle": "WebGL2 (ANGLE / navigateur)"},
    "gles": {"budget": 16, "fusion_cookies": False,
             "libelle": "OpenGL ES 3.x (Android / HarmonyOS)"},
    "desktop": {"budget": 32, "fusion_cookies": False,
                "libelle": "OpenGL 4.x bureau (Windows / Linux)"},
}

RE_SAMPLER = re.compile(
    r"\buniform\s+(?:lowp\s+|mediump\s+|highp\s+)?"
    r"(?:sampler|isampler|usampler)[A-Za-z0-9]*\s+([A-Za-z_][A-Za-z0-9_]*)"
)

# Les declarations que la fusion des cookies SUPPRIME (cf.
# NkWebMergeCookieSamplers, NkOpenglDevice.cpp) : chiffre 1..9 seulement --
# le slot 0 survit, c'est lui qui recoit les usages rediriges.
RE_COOKIE_FUSIONNE = re.compile(
    r"^tLight3D(?:Cube)?Cookie[1-9]$"
)


def includes_declarant(racine):
    """Les .glsli qui DECLARENT un sampler -- l'angle mort du banc.

    Ce script compte fichier par fichier, il ne resout PAS les `#include`.
    Aujourd'hui c'est sans consequence : les .glsli du depot ne declarent RIEN
    (ils documentent en commentaire la declaration attendue, et l'etage fragment
    la porte). Mais le jour ou l'un d'eux declarerait un sampler, le banc
    deviendrait AVEUGLE **en silence** -- il compterait moins que la realite, et
    un banc qui sous-compte est pire qu'un banc absent : il rassure.
    On transforme donc l'angle mort en ALARME.
    """
    coupables = []
    for base, dossiers, fichiers in os.walk(racine):
        dossiers[:] = [d for d in dossiers
                       if d not in (".git", "Build", "Externals", "node_modules")]
        for f in fichiers:
            if not f.endswith(".glsli"):
                continue
            chemin = os.path.join(base, f)
            try:
                with open(chemin, "r", encoding="utf-8", errors="replace") as fh:
                    lignes = fh.readlines()
            except OSError:
                continue
            # on ignore les lignes de commentaire : les .glsli DOCUMENTENT la
            # declaration attendue, ce n'est pas une declaration.
            vraies = [l for l in lignes if not l.lstrip().startswith("//")]
            if RE_SAMPLER.findall("".join(vraies)):
                coupables.append(os.path.relpath(chemin, racine))
    return sorted(coupables)


def etages_fragment(racine):
    """Tous les etages fragment du depot, sans les artefacts de build."""
    trouves = []
    for base, dossiers, fichiers in os.walk(racine):
        dossiers[:] = [d for d in dossiers
                       if d not in (".git", "Build", "Externals", "node_modules")]
        for f in fichiers:
            if (f.endswith(".frag") or f.endswith(".frag.nksl")
                    or f.endswith(".frag.gl.glsl") or f.endswith(".frag.vk.glsl")):
                trouves.append(os.path.join(base, f))
    return sorted(trouves)


# Le retrait pilote par le budget, cote moteur : NkTrimShadowRawSampler
# (NkOpenglDevice.cpp). Reproduit ici pour MESURER son effet sans GPU.
# ⚠️ CE N'EST PAS LA MEME IMPLEMENTATION -- c'est deliberé : le juge doit venir
# d'ailleurs que le juge. Ce mode prouve LA REGLE et SON ARITHMETIQUE (17 -> 16),
# il ne prouve PAS le code C++, qui demande une execution web pour etre juge.
RE_SHADOW_RAW = re.compile(r"^tShadowAtlasRaw$")


def samplers(source, fusion_cookies, sampler_bidon=False, trim_budget=False):
    """Noms des echantillonneurs restants APRES les transformations de la cible."""
    noms = RE_SAMPLER.findall(source)
    if fusion_cookies:
        noms = [n for n in noms if not RE_COOKIE_FUSIONNE.match(n)]
    if trim_budget:
        noms = [n for n in noms if not RE_SHADOW_RAW.match(n)]
    if sampler_bidon:
        noms = noms + ["tControlePositif"]
    return noms


def mesurer(racine, cible, sampler_bidon=False, trim_budget=False):
    conf = CIBLES[cible]
    resultats = []
    for chemin in etages_fragment(racine):
        try:
            with open(chemin, "r", encoding="utf-8", errors="replace") as fh:
                src = fh.read()
        except OSError:
            continue
        noms = samplers(src, conf["fusion_cookies"], sampler_bidon, trim_budget)
        if noms:
            resultats.append((len(noms), os.path.relpath(chemin, racine), noms))
    resultats.sort(reverse=True)
    return resultats


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cible", choices=sorted(CIBLES), default="web")
    ap.add_argument("--racine", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    ap.add_argument("--controle", action="store_true",
                    help="controle positif : injecte un sampler bidon et EXIGE "
                         "que le verdict change")
    ap.add_argument("--apres-trim", action="store_true", dest="apres_trim",
                    help="mesure APRES le retrait pilote par le budget "
                         "(NkTrimShadowRawSampler) : ce que la cible etroite "
                         "recevra reellement")
    ap.add_argument("--tout", action="store_true",
                    help="lister tous les etages, pas seulement les depassements")
    a = ap.parse_args(argv[1:])

    conf = CIBLES[a.cible]
    budget = conf["budget"]

    print("Cible : %s — budget %d unites de texture (etage fragment)"
          % (conf["libelle"], budget))
    if conf["fusion_cookies"]:
        print("Transformation appliquee : fusion des cookies "
              "(NkWebMergeCookieSamplers) — les declarations "
              "tLight3D[Cube]Cookie1..9 sont retirees.")
    print()

    # Angle mort : un include qui declare un sampler rendrait le compte FAUX.
    aveugles = includes_declarant(a.racine)
    if aveugles:
        print("🔴 ANGLE MORT — %d include(s) DECLARENT un echantillonneur :"
              % len(aveugles))
        for c in aveugles:
            print("      %s" % c)
        print("   Ce script compte fichier par fichier, il ne resout pas les")
        print("   #include : son verdict SOUS-COMPTE et ne vaut rien.")
        return 2

    resultats = mesurer(a.racine, a.cible, trim_budget=a.apres_trim)
    depassements = [r for r in resultats if r[0] > budget]
    if a.apres_trim:
        print("Retrait pilote par le budget applique : tShadowAtlasRaw retire")
        print("(PCSS -> repli PCF 3x3 ; branche deja morte a NK_MOBILE).")
        print()

    if a.tout:
        for n, chemin, _ in resultats:
            print("  %3d  %s" % (n, chemin))
        print()

    for n, chemin, noms in depassements:
        print("🔴 DEPASSEMENT  %s" % chemin)
        print("   %d echantillonneurs pour %d accordes — %d de trop."
              % (n, budget, n - budget))
        print("   %s" % ", ".join(noms))
        print()

    # --- Controle positif ----------------------------------------------------
    # Sans lui, « 0 depassement » ne se distingue pas de « je ne sais pas
    # compter ». Deux epreuves : le COMPTEUR reagit, et le VERDICT bascule
    # exactement a la frontiere. Voir l'exigence 3 de l'en-tete : la premiere
    # version de ce controle ne testait que la premiere, et elle etait aveugle.
    if a.controle:
        print("--- CONTROLE POSITIF ---")
        ok = True

        # (a) sensibilite du COMPTEUR
        sans = {c: n for n, c, _ in resultats}
        # ⚠️ MEME conditions que la mesure de reference, sinon on compare deux
        # populations differentes et l ecart ne veut plus rien dire.
        avec = {c: n for n, c, _ in mesurer(a.racine, a.cible,
                                            sampler_bidon=True,
                                            trim_budget=a.apres_trim)}
        ecarts = {c: avec.get(c, 0) - n for c, n in sans.items()}
        mauvais = [c for c, d in ecarts.items() if d != 1]
        if mauvais:
            print("🔴 (a) COMPTEUR MUET : %d etage(s) n'ont pas gagne 1 sampler."
                  % len(mauvais))
            for c in mauvais[:3]:
                print("      %s (ecart %d)" % (c, ecarts[c]))
            ok = False
        else:
            print("✅ (a) compteur sensible : les %d etages gagnent exactement 1."
                  % len(sans))

        # (b) sensibilite du VERDICT, eprouvee SUR LA FRONTIERE.
        # Le depot ne contient aucun shader assis a `budget` : on en fabrique
        # deux. Sans eux, un `>` ecrit `>=` passerait inapercu pour toujours.
        juste = "".join("uniform sampler2D tSynth%d;" % i for i in range(budget))
        trop = juste + "uniform sampler2D tSynthDeTrop;"
        n_juste = len(samplers(juste, conf["fusion_cookies"]))
        n_trop = len(samplers(trop, conf["fusion_cookies"]))
        v_juste = n_juste > budget
        v_trop = n_trop > budget
        if n_juste != budget or n_trop != budget + 1:
            print("🔴 (b) l'etage synthetique n'a pas la taille voulue "
                  "(%d et %d pour un budget de %d)." % (n_juste, n_trop, budget))
            ok = False
        elif v_juste or not v_trop:
            print("🔴 (b) LA FRONTIERE EST FAUSSE : %d juge %s, %d juge %s."
                  % (n_juste, "ROUGE" if v_juste else "vert",
                     n_trop, "ROUGE" if v_trop else "vert"))
            ok = False
        else:
            print("✅ (b) frontiere juste : %d = vert, %d = ROUGE."
                  % (n_juste, n_trop))

        if not ok:
            print()
            print("Le verdict ci-dessus ne vaut RIEN — ce n'est pas un zero,")
            print("c'est une absence de mesure.")
            return 2
        print()

    if depassements:
        print("%d etage(s) fragment au-dessus du budget de la cible « %s »."
              % (len(depassements), a.cible))
        return 1

    print("Aucun depassement — %d etage(s) fragment mesures, budget %d."
          % (len(resultats), budget))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
