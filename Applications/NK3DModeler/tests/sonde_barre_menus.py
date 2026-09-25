#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

LE BANC DE LA BARRE DE MENUS — « un menu qui s'ouvre et se referme dans la
meme image ne s'affiche jamais »
==============================================================================

[!] CE QU'IL EMPECHE DE REVENIR. Rodolf, 25/09/2026 : « les menus du menu
principal ne s'ouvrent pas ». Aucun des sept ne se deroulait, alors que le
menu deroulant du viseur, lui, fonctionnait. Cause mesuree : dans une SEULE
image, le MEME clic etait lu DEUX fois --

  1. `PaintMenuBar` (couche 0) : `hit.Clicked("menu.0")` -> vrai,
     `st.openMenu = 0` ;
  2. plus bas, couche 50, `NkMenuBarClics` redeclarait `menu.0` et relisait le
     MEME clic -- rien ne l'avait consomme -- voyait `st.openMenu == 0`, et
     basculait : **-1**.

Le menu naissait et mourait avant d'etre peint. Sa garde `if (st.openMenu < 0)
return;` etait censee l'en empecher, mais a l'image de l'ouverture cette valeur
venait d'etre ecrite par l'etape 1 : **la garde lisait le resultat de ce
qu'elle devait ignorer.**

⚠️ CE QUE CE BANC MESURE, ET CE QU'IL NE MESURE PAS
   Il passe par `NK_EVENEMENTS`, qui rejoue la souris DANS LE PROCESSUS par les
   memes rappels que Windows. Aucune entree n'est injectee dans le systeme
   d'exploitation. La fenetre s'ouvre sous `NK_SONDE=1` : elle porte alors
   « *** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT *** » et ignore
   toute entree humaine.
   Il ne mesure PAS les pixels : il mesure l'ETAT (`st.openMenu`) et l'EFFET
   (le fichier de disposition ecrit par le clic). Un menu peint mais vide
   passerait -- c'est une limite, elle est dite.

⚠️ LA PREMIERE LECTURE DE CE DEFAUT A ETE FAUSSE, et c'est la lecon du banc :
   la trace montrait `openMenu=0` sur les lignes SUIVANTES de la meme image
   (les entrees i=1..6, imprimees apres l'affectation), et j'en ai conclu « ca
   s'ouvre ». Il fallait regarder l'image D'APRES. C'est pour cela que le
   critere b1 compte des IMAGES, pas des occurrences.

Usage :
    python Applications/NK3DModeler/tests/sonde_barre_menus.py
    python ... --exe <binaire d'AVANT le correctif> --negatif
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
EXE_DEFAUT = os.path.join(RACINE, "Build", "Bin", "Release-Windows", "NK3DModeler",
                          "NK3DModeler.exe")

# Geometrie relevee par la sonde elle-meme (NK_MENU_TRACE=2, balayage), a
# l'echelle 1. Elles sont ECRITES ICI parce qu'elles sont MESUREES : les
# recalculer donnerait une seconde verite.
CLIC_FICHIER = (60, 15)     # menu.0, rect mesure 45,7,60,21
CLIC_FENETRE = (196, 15)    # menu.2, rect mesure 164,7,65,21

# ⚠️ LA POSITION DE L'ENTREE « Compteurs de rendu » N'EST PAS ECRITE ICI, et
#    c'est une correction que ce banc a payee. Elle l'etait -- (210, 235), le
#    rang 9 du menu Fenetre -- et retirer UNE entree perimee du menu a decale
#    la cible : le banc aurait rougi pour une raison qui n'a rien a voir avec
#    ce qu'il mesure. C'est l'APPLICATION qui dit ou elle est
#    (`NK_MENU_TRACE=2` imprime « entree menu=.. cle=.. rect=.. »), et le banc
#    la lui demande. *Un indice n'est pas un nom.*
CLE_COMPTEURS = "app.compteurs"

gOk = 0
gKo = 0


def verdict(ident, cond, quoi):
    global gOk, gKo
    if cond:
        gOk += 1
        print("  [ ok ] %-4s %s" % (ident, quoi))
    else:
        gKo += 1
        print("  [FAIL] %-4s %s" % (ident, quoi))


def courir(exe, script, images, etat_dir, trace=1):
    """Lance UNE course et rend son journal. La disposition part dans
    `etat_dir` : la sonde ne touche jamais le fichier de l'utilisateur."""
    env = dict(os.environ)
    env["NK_SONDE"] = "1"
    env["NK_ETAT_SONDE"] = etat_dir
    env["NK_MENU_TRACE"] = str(trace)
    env["NK_PROJECT"] = os.path.join(etat_dir, "projet", "projet", "projet.nk3dm")
    env["NK_EVENEMENTS"] = script
    env["NK_AGENT_EXIT"] = str(images)
    r = subprocess.run([exe], env=env, cwd=RACINE, capture_output=True, text=True,
                       errors="replace", timeout=300)
    # ⚠️ LE CODE DE SORTIE N'EST PAS LU. Il ment dans ce depot (une application a
    #    fenetre rend 127 sous l'outil Bash, et `jenga` rend 0 sur un echec). Le
    #    verdict est dans ce qui est ECRIT.
    return r.stdout + r.stderr


def images_menu_ouvert(journal, valeur):
    """Nombre d'IMAGES distinctes ou `openMenu` vaut `valeur`. On compte des
    images et non des lignes : plusieurs lignes d'une meme image portent l'etat
    de milieu d'image, et c'est exactement ce qui m'a fait conclure de travers
    la premiere fois."""
    vues = set()
    for m in re.finditer(r"img=(\d+) .*openMenu=(-?\d+)", journal):
        if int(m.group(2)) == valeur:
            vues.add(int(m.group(1)))
    return len(vues)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=EXE_DEFAUT)
    ap.add_argument("--negatif", action="store_true",
                    help="attend b1 et b4 ROUGES (binaire d'avant le correctif)")
    a = ap.parse_args()

    if not os.path.isfile(a.exe):
        print("binaire introuvable : %s" % a.exe)
        return 2
    print("=== banc de la barre de menus -> %s ===" % a.exe)

    base = tempfile.mkdtemp(prefix="nk3d_menus_")

    # ── b1 : UN CLIC OUVRE, ET LE MENU RESTE OUVERT ─────────────────────────
    j1 = courir(a.exe, "60:m:60:15;80:d:60:15;82:u:60:15", 150, os.path.join(base, "b1"),
                trace=2)
    n1 = images_menu_ouvert(j1, 0)
    verdict("b1", n1 >= 20,
            "un clic sur « Fichier » laisse le menu ouvert (%d images >= 20)" % n1)

    # ── b2 : LE NEGATIF DE b1 -- sans clic, rien ne s'ouvre ─────────────────
    #    Sans lui, b1 passerait sur un binaire qui ouvrirait un menu tout seul.
    j2 = courir(a.exe, "60:m:60:15", 150, os.path.join(base, "b2"), trace=2)
    n2 = images_menu_ouvert(j2, 0)
    verdict("b2", n2 == 0,
            "NEGATIF : survol SANS clic -> aucun menu ne s'ouvre (%d images)" % n2)

    # ── b3 : PASSER D'UN MENU A L'AUTRE ─────────────────────────────────────
    #    C'est le role meme de `NkMenuBarClics`. Si le correctif l'avait
    #    neutralisee, b1 passerait et b3 tomberait : le correctif serait pire
    #    que le defaut.
    j3 = courir(a.exe, "60:m:60:15;80:d:60:15;82:u:60:15;"
                       "100:m:196:15;120:d:196:15;122:u:196:15", 190,
                os.path.join(base, "b3"), trace=2)
    n3 = images_menu_ouvert(j3, 2)
    verdict("b3", n3 >= 20 and "NkMenuBarClics i=2 openMenu 0 -> 2" in j3,
            "un second clic passe de « Fichier » a « Fenetre » (%d images sur le 2)" % n3)

    # ── b4 : BOUT EN BOUT -- l'entree du menu AGIT ──────────────────────────
    #    « Fenetre -> Compteurs de rendu » bascule l'interrupteur, qui s'ecrit
    #    dans le fichier de disposition. On lit le FICHIER : c'est l'effet, pas
    #    l'intention.
    etat4 = os.path.join(base, "b4")
    cfg = os.path.join(etat4, "nk3dmodeler_ui.cfg")
    # (i) on ouvre le menu et on DEMANDE a l'application ou est l'entree ;
    repere = courir(a.exe, "60:m:196:15;80:d:196:15;82:u:196:15", 140,
                    os.path.join(base, "b4a"), trace=2)
    cible = None
    for m in re.finditer(r"entree menu=2 i=\d+ cle=(\S+) libelle=.* "
                         r"rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)", repere):
        if m.group(1) == CLE_COMPTEURS:
            x, y, w, h = (int(m.group(k)) for k in (2, 3, 4, 5))
            cible = (x + w // 2, y + h // 2)
    verdict("b4a", cible is not None,
            "l'application PUBLIE la position de « %s » %s" %
            (CLE_COMPTEURS, ("-> %s" % (cible,)) if cible else "(introuvable)"))
    # (ii) puis on clique LA.
    if cible:
        courir(a.exe, "60:m:196:15;80:d:196:15;82:u:196:15;"
                      "100:m:%d:%d;120:d:%d:%d;122:u:%d:%d"
                      % (cible[0], cible[1], cible[0], cible[1], cible[0], cible[1]),
               170, etat4)
    ecrit = ""
    if os.path.isfile(cfg):
        with open(cfg, "r", encoding="utf-8", errors="replace") as f:
            ecrit = f.read()
    verdict("b4", "compteurs=1" in ecrit,
            "« Fenetre -> Compteurs de rendu » allume ET ecrit la disposition")

    print("RESULTAT : %d/%d" % (gOk, gOk + gKo))
    if a.negatif:
        # Le negatif NOMME les criteres attendus en rouge : un banc casse pour
        # une autre raison ne doit pas pouvoir se faire passer pour une
        # refutation reussie.
        # Sur le binaire d AVANT, le menu ne s ouvre pas : aucune entree n est
        # donc publiee non plus. b4a rougit pour la MEME cause que b1 et b4, et
        # c est dit ici plutot que decouvert en lisant le rouge.
        attendus_rouges = (n1 < 20) and (cible is None) and ("compteurs=1" not in ecrit)
        print("NEGATIF %s : b1, b4a et b4 devaient rougir." %
              ("CONFORME" if attendus_rouges else "NON CONFORME"))
        return 0 if attendus_rouges else 1
    return 0 if gKo == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
