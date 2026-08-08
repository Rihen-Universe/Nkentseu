#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Genere la bibliotheque de plateaux livree avec ConquerorLab.

POURQUOI UN SCRIPT ET PAS HUIT FICHIERS ECRITS A LA MAIN
--------------------------------------------------------
La conversion odd-r -> axial (`q = col - (row >> 1)`) est LE piege des plateaux
hexagonaux : se tromper d'une ligne donne un losange penche au lieu d'un
rectangle d'hexagones, et l'erreur ne se voit qu'a l'ecran. Ecrite une fois ici,
elle est juste pour tous les plateaux.

Le script est aussi la garantie de la SYMETRIE exigee par REGLES 4.2 : les
positions de depart sont calculees par reflexion, jamais posees a l'oeil. Un
plateau asymetrique rend tout ecart de winrate ininterpretable.

    python _generer.py
"""

import json
import os

ICI = os.path.dirname(os.path.abspath(__file__))


def ecrire(nom, desc, commentaire):
    """Ecrit un plateau. Le JSON reste indente : ces fichiers sont faits pour
    etre ouverts et modifies a la main par un stagiaire."""
    with open(os.path.join(ICI, nom), "w", encoding="utf-8") as f:
        f.write(json.dumps(desc, ensure_ascii=False, indent=2))
        f.write("\n")
    n = len(desc["cells"])
    b = len(desc.get("blocked", []))
    bloq = f", {b} bloquees" if b else ""
    print(f"  {nom:<30} {n:>3} cases{bloq:<16} {commentaire}")


def hex_rect(cols, rows):
    """Rectangle d'hexagones pointy-top, en coordonnees AXIALES."""
    return [[col - (row >> 1), row] for row in range(rows) for col in range(cols)]


def deux_coins(cells):
    """Un totem par joueur, aux deux extremites de la liste — diagonalement
    opposees sur toutes les formes generees ici, donc symetriques."""
    a, b = cells[0], cells[-1]
    return [{"player": 0, "q": a[0], "r": a[1], "level": 0},
            {"player": 1, "q": b[0], "r": b[1], "level": 0}]


def quatre_coins(cells):
    """Deux totems par joueur : la disposition du plateau de reference."""
    n = len(cells)
    idx = [0, n - 1, min(5, n - 1), max(0, n - 6)]
    owners = [0, 0, 1, 1]
    return [{"player": o, "q": cells[i][0], "r": cells[i][1], "level": 0}
            for i, o in zip(idx, owners)]


def plateau(topology, cells, starts, blocked=None, cell_shape=None):
    """`cell_shape` est de la PRESENTATION : "SQUARE", "HEX" ou "CIRCLE".

    Elle ne touche ni au voisinage, ni aux coups legaux, ni au resultat — c'est
    justement pourquoi elle n'est PAS dans le contrat : le moteur ne la lit
    jamais, seul l'atelier la regarde pour dessiner. Absente, l'atelier deduit
    la forme de la topologie, comme avant.
    """
    d = {"topology": topology, "cells": cells, "blocked": blocked or [],
         "starts": starts, "min_players": 2, "max_players": 2}
    if cell_shape:
        d["cell_shape"] = cell_shape
    return d


def parallelogramme(cols, rows):
    """Parallelogramme d'hexagones : la forme NATURELLE des coordonnees axiales.

    C'est `hex_rect` SANS la correction `- (row >> 1)`. Autrement dit : la forme
    qu'on obtient quand on oublie la conversion odd-r -> axial. La montrer comme
    un plateau a part entiere vaut mieux que de la laisser apparaitre comme un
    bug — un stagiaire qui la reconnait a l'ecran comprend du meme coup ce que
    corrige `hex_rect`.
    """
    return [[col, row] for row in range(rows) for col in range(cols)]


def croix(bras, epaisseur=1):
    """Croix grecque : deux bras qui se croisent au centre."""
    r = bras
    return [[x, y] for y in range(-r, r + 1) for x in range(-r, r + 1)
            if abs(x) <= epaisseur or abs(y) <= epaisseur]


print("Plateaux generes :\n")

# ── HEXAGONES ────────────────────────────────────────────────────────────────
c = hex_rect(6, 7)
ecrire("hexagone_6x7.json", plateau("HEX_POINTY", c, quatre_coins(c)),
       "le plateau de reference")

c = hex_rect(4, 5)
ecrire("hexagone_4x5.json", plateau("HEX_POINTY", c, deux_coins(c)),
       "petit : parties courtes, iteration rapide")

c = hex_rect(9, 8)
ecrire("hexagone_9x8.json", plateau("HEX_POINTY", c, quatre_coins(c)),
       "grand : la duplication a le temps de respirer")

# Le meme 6x7, coeur bloque : que devient le jeu quand la zone la mieux
# connectee du plateau disparait ?
c = hex_rect(6, 7)
coeur = [[col - (row >> 1), row] for row in (2, 3, 4) for col in (2, 3)]
ecrire("hexagone_coeur_bloque.json",
       plateau("HEX_POINTY", c, quatre_coins(c), coeur),
       "le centre ne vaut plus rien")

c = hex_rect(6, 7)
ecrire("hexagone_plat.json", plateau("HEX_FLAT", c, quatre_coins(c)),
       "meme forme, hexagones tournes")

# ── CARRES ───────────────────────────────────────────────────────────────────
c = [[x, y] for y in range(6) for x in range(8)]
ecrire("rectangle_8x6.json", plateau("SQUARE_4", c, deux_coins(c)),
       "4 voisins : le voisinage le plus pauvre")

c = [[x, y] for y in range(8) for x in range(8)]
ecrire("carre_8x8_diagonales.json", plateau("SQUARE_8", c, deux_coins(c)),
       "8 voisins : cascades bien plus larges")

# Diamant : |x| + |y| <= 4. Non rectangulaire — et c'est le point : `cells`
# definit la FORME REELLE, pas une boite englobante.
c = [[x, y] for y in range(-4, 5) for x in range(-4, 5) if abs(x) + abs(y) <= 4]
ecrire("diamant.json", plateau("SQUARE_4", c, deux_coins(c)),
       "non rectangulaire : cells definit la forme")

# Croix : deux bras etroits qui se rejoignent. La position y compte beaucoup
# plus que le nombre.
c = croix(4, 1)
ecrire("plus.json", plateau("SQUARE_4", c, deux_coins(c)),
       "goulots : la position prime sur le nombre")

# ── PARALLELOGRAMMES ─────────────────────────────────────────────────────────
# La forme naturelle des coordonnees axiales : hex_rect SANS la correction
# odd-r. Un stagiaire qui la reconnait a l'ecran comprend du meme coup ce que
# corrige hex_rect — c'est plus instructif qu'un paragraphe de documentation.
c = parallelogramme(6, 7)
ecrire("parallelogramme_6x7.json", plateau("HEX_POINTY", c, quatre_coins(c)),
       "hexagones sans correction odd-r")

c = parallelogramme(8, 5)
ecrire("parallelogramme_8x5.json", plateau("HEX_POINTY", c, deux_coins(c)),
       "large et plat : les bras s'allongent")

# ── FORMES DE CELLULE ────────────────────────────────────────────────────────
# Meme grille, meme voisinage, meme regles — SEUL le dessin change. C'est le
# meilleur moyen de constater que la forme dessinee et la topologie sont deux
# choses distinctes, et de juger laquelle se LIT le mieux.
c = [[x, y] for y in range(6) for x in range(8)]
ecrire("rectangle_8x6_rond.json",
       plateau("SQUARE_4", c, deux_coins(c), cell_shape="CIRCLE"),
       "meme grille, cellules rondes")

c = hex_rect(6, 7)
ecrire("hexagone_6x7_rond.json",
       plateau("HEX_POINTY", c, quatre_coins(c), cell_shape="CIRCLE"),
       "le plateau de reference, en pastilles")

c = [[x, y] for y in range(8) for x in range(8)]
ecrire("carre_8x8_hexagones.json",
       plateau("SQUARE_8", c, deux_coins(c), cell_shape="HEX"),
       "8 voisins, dessine en hexagones")

print("\nCharger dans l'atelier : panneau Regles -> Plateau -> Grille.")
print("La forme des cellules se change aussi a la volee, juste en dessous.")
