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
                    help="attend b1, b4a et b4 ROUGES (binaire d'avant le correctif)")
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
    # ⚠️ LE MOTIF NE NOMME QUE CE QU'IL LIT. Sa premiere version epelait toute la
    #    ligne (« cle=.. libelle=.. rect=.. ») ; ajouter deux champs a la trace
    #    (`indispo=`, `racc=`) l'a fait cesser de correspondre, et b4a a rougi pour
    #    une raison sans rapport avec ce qu'il mesure. *Un instrument dont on change
    #    le format casse ses lecteurs en silence* -- le banc, lui, l'a dit.
    for m in re.finditer(r"entree menu=2 .*cle=(\S+) .*rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)",
                         repere):
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

    # ── b5..b7 : « PREVU, PAS DISPONIBLE » ──────────────────────────────────
    #    Une entree dont le panneau n'existe pas est GRISEE : elle dit la verite.
    #    La retirer effacerait une intention reelle (`app.panneau_outils` est liee
    #    a la touche T) ; la laisser vive PROMETTRAIT une fonction absente.
    etat5 = os.path.join(base, "b5")
    j5 = courir(a.exe, "60:m:196:15;80:d:196:15;82:u:196:15", 140, etat5, trace=2)
    ind = re.search(r"cle=app\.panneau_outils indispo=(\d) racc=(\S+) .*"
                    r"rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)", j5)
    verdict("b5", bool(ind) and ind.group(1) == "1",
            "« Panneau d'outils » est declaree INDISPONIBLE")
    # ⚠️ `racc` dit le raccourci REELLEMENT PEINT. Le critere est refutable :
    #    mesure faite en retirant la garde -- il imprime alors « racc=T ».
    verdict("b6", bool(ind) and ind.group(2) == "-",
            "et son raccourci n'est PAS affiche (sans la garde : « T »)")
    if ind:
        x, y, w, h = (int(ind.group(k)) for k in (3, 4, 5, 6))
        cx, cy = x + w // 2, y + h // 2
        j7 = courir(a.exe, "60:m:196:15;80:d:196:15;82:u:196:15;"
                           "100:m:%d:%d;120:d:%d:%d;122:u:%d:%d"
                           % (cx, cy, cx, cy, cx, cy), 190,
                    os.path.join(base, "b7"), trace=2)
        # INERTE, PAS ABSENTE : le menu reste ouvert. S'il se refermait, le clic
        # « aurait fait quelque chose » -- et c'est ce que le gris doit nier.
        reste = images_menu_ouvert(j7, 2)
        verdict("b7", reste >= 60,
                "cliquer dessus ne fait RIEN et ne referme pas le menu (%d images)" % reste)
    else:
        verdict("b7", False, "entree introuvable : b7 non mesure")

    # ── b8/b9 : LA PAIRE APPARIEE, dans LE MEME menu ────────────────────────
    #    « Objet » porte les deux cas : « Deplacer » est cablee, « Appliquer
    #    tout » est grisee. Cliquer la premiere REFERME le menu, cliquer la
    #    seconde le LAISSE OUVERT. Les deux clics sont a quelques pixels l'un de
    #    l'autre, dans le meme menu, a la meme image : la difference ne peut
    #    venir que du champ `indisponible`. *Un negatif apparie vaut mieux qu'un
    #    negatif lointain.*
    CLIC_OBJET = (400, 15)
    rep2 = courir(a.exe, "60:m:400:15;80:d:400:15;82:u:400:15", 140,
                  os.path.join(base, "b8a"), trace=2)
    cibles = {}
    for m in re.finditer(r"entree menu=5 .*cle=(\S+) indispo=(\d) .*"
                         r"rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)", rep2):
        x, y, w, h = (int(m.group(k)) for k in (3, 4, 5, 6))
        cibles[m.group(1)] = (m.group(2), x + w // 2, y + h // 2)
    # « Appliquer tout » n'a pas de cle : on la repere par son rang DANS la
    # trace, qui donne aussi le libelle. Plus simple : on prend la derniere
    # entree sans cle et indisponible du menu Objet.
    grisee = None
    for m in re.finditer(r"entree menu=5 .*indispo=1 racc=\S+ libelle=Appliquer tout "
                         r"rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)", rep2):
        x, y, w, h = (int(m.group(k)) for k in (1, 2, 3, 4))
        grisee = (x + w // 2, y + h // 2)

    dep = cibles.get("objet.deplacer")
    if dep and dep[0] == "0":
        j8 = courir(a.exe, "60:m:400:15;80:d:400:15;82:u:400:15;"
                           "100:m:%d:%d;120:d:%d:%d;122:u:%d:%d"
                           % (dep[1], dep[2], dep[1], dep[2], dep[1], dep[2]), 190,
                    os.path.join(base, "b8"), trace=2)
        ouvert8 = images_menu_ouvert(j8, 5)
        verdict("b8", ouvert8 < 60,
                "« Objet -> Deplacer » (cablee) AGIT et referme le menu (%d images)" % ouvert8)
    else:
        verdict("b8", False, "« objet.deplacer » introuvable ou grisee")

    if grisee:
        j9 = courir(a.exe, "60:m:400:15;80:d:400:15;82:u:400:15;"
                           "100:m:%d:%d;120:d:%d:%d;122:u:%d:%d"
                           % (grisee[0], grisee[1], grisee[0], grisee[1], grisee[0], grisee[1]),
                    190, os.path.join(base, "b9"), trace=2)
        ouvert9 = images_menu_ouvert(j9, 5)
        verdict("b9", ouvert9 >= 60,
                "NEGATIF APPARIE : « Appliquer tout » (grisee) laisse le menu ouvert "
                "(%d images)" % ouvert9)
    else:
        verdict("b9", False, "« Appliquer tout » introuvable")

    # ── b10..b12 : « AIDE -> A PROPOS », et c'est une OBLIGATION JURIDIQUE ──
    #    62 icones de `data/icons/` sont une copie de vscode-codicons sous
    #    CC BY 4.0, qui EXIGE l'attribution. Une attribution que seul un
    #    developpeur peut lire ne remplit pas la condition : elle doit etre
    #    ATTEIGNABLE DEPUIS L'APPLICATION. Ces criteres verifient le chemin.
    CLIC_AIDE = (430, 15)
    rep3 = courir(a.exe, "60:m:430:15;80:d:430:15;82:u:430:15", 140,
                  os.path.join(base, "b10a"), trace=2)
    cibleAp = None
    for m in re.finditer(r"entree menu=6 .*cle=(\S+) indispo=(\d) .*"
                         r"rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)", rep3):
        if m.group(1) == "app.apropos":
            x, y, w, h = (int(m.group(k)) for k in (3, 4, 5, 6))
            cibleAp = (m.group(2), x + w // 2, y + h // 2)
    verdict("b10", bool(cibleAp) and cibleAp[0] == "0",
            "« A propos » est DEGRISEE (la mention doit etre atteignable)")

    if cibleAp:
        cx, cy = cibleAp[1], cibleAp[2]
        j11 = courir(a.exe, "60:m:430:15;80:d:430:15;82:u:430:15;"
                            "100:m:%d:%d;120:d:%d:%d;122:u:%d:%d" % (cx, cy, cx, cy, cx, cy),
                     190, os.path.join(base, "b11"), trace=2)
        ouverts = len([1 for l in j11.splitlines() if "apropos=1" in l])
        # ⚠️ ON COMPTE DES IMAGES, pas des occurrences -- et pour la raison
        #    exacte du 25/09 : l'ecran s'ouvrait ET se refermait dans la MEME
        #    image (son voile, en couche 150, avalait le clic qui venait de
        #    l'ouvrir). Un critere qui regarderait l'image du clic passerait.
        verdict("b11", ouverts >= 20,
                "un clic l'OUVRE et il RESTE ouvert (%d images) -- persistant, pas un bandeau"
                % ouverts)
        # Le menu se referme, comme toute entree ACTIVE : c'est ce qui la
        # distingue d'une entree grisee (b7).
        # ⚠️ LE CRITERE DIT CE QU'IL VEUT DIRE, ET NON UN SEUIL CALIBRE.
        #    Premiere version : « moins de 40 images ouvertes au total » -- elle
        #    a rougi, et la mesure a montre que le PRODUIT etait juste : le menu
        #    EST ferme apres le clic, simplement le scenario laisse le menu
        #    ouvert ~40 images AVANT. J'ai reformule au lieu d'ajuster le
        #    nombre : *un seuil qu'on deplace jusqu'a ce qu'il passe ne mesure
        #    plus rien.* On regarde donc les images QUI SUIVENT le clic.
        apres = [int(m.group(1)) for m in
                 re.finditer(r"img=(\d+) .*openMenu=6", j11) if int(m.group(1)) > 130]
        verdict("b12", len(apres) == 0,
                "et le menu Aide est FERME apres le clic (%d image(s) encore ouverte(s))"
                % len(apres))
    else:
        verdict("b11", False, "« A propos » introuvable")
        verdict("b12", False, "« A propos » introuvable")

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
