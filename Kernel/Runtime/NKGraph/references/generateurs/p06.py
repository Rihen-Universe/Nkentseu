# -*- coding: utf-8 -*-
# Planche 06 -- les elements decides dans la nuit du 22 au 23/08, et qui
# n existaient nulle part en image. Rodolf a passe la nuit a decider SUR DU
# TEXTE : cette planche est ce qu il regarde au reveil.
#
# Elle ne montre QUE du neuf. Rien de ce qui est deja sur les planches 01-05
# n y est repris.
import sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from gen import *

# Les ordonnees des rangees sont CALCULEES a partir de la hauteur reelle des
# cartouches (17 px par ligne + 28 d entete + 8), pas estimees a l oeil : la
# premiere version faisait chevaucher le panneau 4 et le cartouche 2, et seul
# le PNG regarde l a montre.
W, H = 1760, 1320
s = head(W, H, u'Planche 06 — ce qui a été décidé dans la nuit du 22 au 23/08',
         u'le fil d’Ariane · le fil sélectionné (mesuré) · le nœud indisponible · le nœud survolé · les TROIS GRIS côte à côte')


def lab(x, y, n, titre, note=u''):
    return panneau(u'%s_%s'%(n,titre))+tt(x, y, u'%s · %s' % (n, titre), ORANGE, 12.5, '600') + tt(x, y + 16, note, TXT3, 10)


def eclaircir(h, k=0.35):
    """+35 % vers le blanc -- la valeur MESUREE au § 10."""
    h = h.lstrip('#')
    c = [int(h[i:i + 2], 16) for i in (0, 2, 4)]
    return '#%02X%02X%02X' % tuple(int(round(v + (255 - v) * k)) for v in c)


RANGS = [{'lab': u'Valeur A', 'coul': FAM['nombre'], 'plein': True, 'glyphe': '1.0', 'val': '0.500'},
         {'lab': u'Valeur B', 'coul': FAM['nombre'], 'plein': True, 'glyphe': '1.0', 'val': '2.000'},
         {'lab': u'Résultat', 'coul': FAM['nombre'], 'plein': True, 'sortie': True, 'glyphe': '1.0'}]

# ── 1 · LE FIL D’ARIANE ──────────────────────────────────────────────────────
R1 = 104
s += lab(34, R1, '1', u'LE FIL D’ARIANE — obligatoire depuis le 23/08',
         u'entrer dans un groupe est le SEUL moyen de l’éditer ; sans lui on ne sait ni où l’on est, ni comment sortir')

BX, BY, BW = 34, R1 + 34, 820
s += '<rect x="%s" y="%s" width="%s" height="24" rx="3" fill="#1E1E24" stroke="%s"/>\n' % (BX, BY, BW, FILET)
s += tt(BX + 12, BY + 16, u'Matériau mur', u'#9FB4C8', 12)
s += tt(BX + 104, BY + 16, u'▸', TXT3, 12)
s += tt(BX + 120, BY + 16, u'Bruit de surface', u'#EEF2F6', 12, '600')
s += '<rect x="%s" y="%s" width="30" height="15" rx="7.5" fill="%s" opacity="0.22"/>\n' % (BX + 228, BY + 4.5, ORANGE)
s += tt(BX + 243, BY + 16, u'⑶', ORANGE, 11, '600', 'middle')
# les trois actions, a DROITE de la meme bande (E4 : aucune barre d outils)
for i, (t, w_) in enumerate([(u'cadrer tout', 78), (u'zoom 100 %', 74), (u'55 %', 40)]):
    x = BX + BW - 12 - sum(v for _, v in [(u'cadrer tout', 86), (u'zoom 100 %', 82), (u'55 %', 48)][i:])
    s += '<rect x="%s" y="%s" width="%s" height="16" rx="2" fill="#26262C"/>\n' % (x, BY + 4, w_)
    s += tt(x + w_ / 2.0, BY + 15.5, t, TXT2 if i < 2 else ORANGE, 10, None, 'middle')

s += tt(BX + 250, BY + 46, u'⑶ le COMPTEUR D’INSTANCES — et ce n’est pas de la décoration :', ORANGE, 10.5, '600')
s += tt(BX + 250, BY + 60, u'on entre par UNE instance, mais on édite la DÉFINITION, qui est PARTAGÉE.', TXT3, 10)
s += tt(BX + 250, BY + 73, u'Sans lui, on croit corriger un nœud et on en corrige trois.', TXT3, 10)
s += tt(BX, BY + 46, u'segments cliquables —', TXT3, 10)
s += tt(BX, BY + 59, u'chacun remonte d’un niveau', TXT3, 10)
s += tt(BX + BW - 200, BY + 46, u'les 3 actions d’E4 vivent ICI :', TXT3, 10)
s += tt(BX + BW - 200, BY + 59, u'aucune barre d’outils séparée', TXT3, 10)

# elision au milieu, au-dela de 4 niveaux
EY = BY + 96
s += '<rect x="%s" y="%s" width="%s" height="24" rx="3" fill="#1E1E24" stroke="%s"/>\n' % (BX, EY, BW, FILET)
for x, t, c, w_ in [(12, u'Racine', u'#9FB4C8', None), (72, u'▸', TXT3, None), (88, u'…', TXT2, '600'),
                    (108, u'▸', TXT3, None), (124, u'Bruit', u'#9FB4C8', None),
                    (172, u'▸', TXT3, None), (188, u'Détail', u'#EEF2F6', '600')]:
    s += tt(BX + x, EY + 16, t, c, 12, w_)
s += tt(BX + 250, EY + 16, u'au-delà de 4 niveaux : on élide AU MILIEU, jamais au début ni à la fin —', TXT3, 10)
s += tt(BX + 250, EY + 29, u'l’origine et l’endroit où l’on est sont les deux seules choses qu’on cherche.', TXT3, 10)

c1, h1 = cartouche(34, EY + 52, 820, [
    u'✅ DÉCIDÉ 23/08 · bande de 24 px, TOUJOURS présente. Le chemin est celui de l’INSTANCIATION, pas de la définition.',
    u'✅ DÉCIDÉ 23/08 · entrer dans un groupe se fait SUR PLACE, jamais en onglet — deux mécanismes de navigation pour',
    u'   une seule notion, c’est un de trop. Deux chemins pour entrer, c’est deux chemins pour se perdre.',
    u'📌 Le modèle le porte déjà : NkEvalStep::path produit « racine/Instance 1 ». Rien à inventer côté données.',
], u'ce que la bande porte, et pourquoi')
s += c1

# ── 2 · LE FIL SÉLECTIONNÉ ───────────────────────────────────────────────────
X2 = 900
s += lab(X2, R1, '2', u'LE FIL SÉLECTIONNÉ — MESURÉ DEUX FOIS, la luminosité a perdu deux fois',
         u'les deux poignées sont le SEUL signal — l’éclaircissement a été mesuré, puis RETIRÉ')

FY = R1 + 40
cas = [(u'non sélectionné', FAM['nombre'], None, 2),
       (u'SÉLECTIONNÉ — deux poignées, AUCUN éclaircissement', FAM['nombre'], 'poignees', 2),
       (u'SÉLECTIONNÉ, prises HORS CADRE — chevrons sur le bord', FAM['nombre'], 'bord', 2),
       (u'fil d’exécution (non sélectionné)', FAM['exec'], None, 3.5)]
for i, (nom, coul, marque, ep) in enumerate(cas):
    y = FY + i * 56
    if marque == 'bord':
        # le fil TRAVERSE : il entre et sort du cadre visible, ses deux prises
        # sont dehors -- et la poignee ne disparait pas pour autant.
        s += '<rect x="%s" y="%s" width="300" height="40" fill="none" stroke="#2B2B33" stroke-dasharray="3 3"/>\n' % (X2, y - 9)
        s += fil(X2 - 34, y, X2 + 334, y + 22, coul, ep)
        for bx, sens, by in ((X2, -1, y + 2), (X2 + 300, 1, y + 20)):
            s += '<path d="M%s %s l%s -5 l0 10 z" fill="%s"/>\n' % (bx + sens * 9, by, -sens * 9, coul)
    else:
        s += fil(X2, y, X2 + 300, y + 22, coul, ep)
        if marque == 'poignees':
            for px, py in ((X2, y), (X2 + 300, y + 22)):
                s += '<rect x="%s" y="%s" width="7" height="7" rx="1" fill="%s"/>\n' % (px - 3.5, py - 3.5, coul)
    s += tt(X2 + 352, y + 16, nom, TXT2 if marque is None else u'#EEF2F6', 10.5, '600' if marque else None)

s += tt(X2, FY + 226, u'⚠ pourquoi l’épaisseur était disqualifiée d’avance :', ORANGE, 10.5, '600')
s += tt(X2, FY + 240, u'un fil de donnée (2 px) épaissi de 60 % donne 3,2 px —', TXT3, 10)
s += tt(X2, FY + 253, u'soit l’épaisseur d’un fil d’EXÉCUTION non sélectionné (3,5).', TXT3, 10)

c2, h2 = cartouche(X2, FY + 272, 820, [
    u'🔴 MESURÉ · AUCUNE valeur de luminosité ne convient. Écart perçu / séparation entre familles (plancher = 11,0) :',
    u'     +20 % → 2,74 / 17,25 (imperceptible)   ·   +35 % → 5,23 / 14,26   ·   +50 % → 8,21 / 11,42 (0,42 de marge)',
    u'     +65 % → 11,81 / 7,96 ❌ le premier taux AUSSI LISIBLE qu’un écart de type CASSE le code couleur des types.',
    u'❌ ET LE RENFORT EST TOMBÉ AUSSI : à +35 %, même en comparaison SIMULTANÉE (5 fils identiques, un seul éclairci,',
    u'   poignées hors cadre), on ne repère PAS le bon fil. Un renfort imperceptible est un DÉCOR — retiré le 23/08.',
    u'✅ CE QUI LE REMPLACE est géométrique, pas chromatique : LA POIGNÉE NE DISPARAÎT JAMAIS. Quand la prise sort du',
    u'   cadre, elle se pose là où le fil COUPE le bord, en CHEVRON vers l’extérieur — 3ᵉ ligne ci-dessus. Il reste',
    u'   à l’écran, et sa POINTE dit de quel côté est la prise : le signal le plus fort reste le seul qui informe.',
    u'📌 Et elles informent : elles DÉSIGNENT LES DEUX PRISES RELIÉES. Un halo dirait « ce fil est sélectionné » ;',
    u'   les poignées disent « ce fil relie CETTE prise à CELLE-LÀ ». On ne remplace pas un signal qui informe par un décor.',
    u'⚠ Le maillon faible n’est pas #9AA3AD mais #81EBEB : ce n’est pas la clarté qui résiste, c’est la SATURATION.',
], u'le refus chiffré, et ce qu’il rend défendable')
s += c2

# ── 3 · LE NŒUD INDISPONIBLE ─────────────────────────────────────────────────
R2 = 640
s += lab(34, R2, '3', u'LE NŒUD INDISPONIBLE — le rang interdit',
         u'Light Path, Raycast, Ambient Occlusion… ils supposent un tracé de rayons ; notre moteur rastérise')

s += tt(34, R2 + 44, u'dans la BIBLIOTHÈQUE', TXT2, 11, '600')
s += '<rect x="34" y="%s" width="300" height="44" rx="3" fill="#1E1E24" stroke="%s" opacity="0.5"/>\n' % (R2 + 54, FILET)
s += tt(48, R2 + 74, u'Light Path', TXT, 12, None, None, 0.5)
s += tt(48, R2 + 89, u'nécessite un tracé de rayons', TXT3, 9.5)
s += tt(34, R2 + 118, u'PRÉSENT, à 50 % — un nœud absent se cherche', TXT3, 10)
s += tt(34, R2 + 131, u'indéfiniment ; un nœud refusé se comprend une fois.', TXT3, 10)

s += tt(370, R2 + 44, u'POSÉ SUR LE CANEVAS (fichier importé)', TXT2, 11, '600')
n, h = noeud(370, R2 + 54, 250, [{'lab': u'Rayon', 'coul': FAM['geom'], 'plein': True, 'glyphe': 'XYZ'},
                                 {'lab': u'Est-il éclairé', 'coul': FAM['nombre'], 'plein': True, 'sortie': True, 'glyphe': 'V/F'}],
             u'Light Path', u'Entrée', CAT['entree'], hachure=True, opacite=0.72)
s += n
s += tt(370, R2 + 54 + h + 16, u'hachures de CORPS · en-tête à 50 % · prises et fils INTACTS', TXT3, 10)
s += tt(370, R2 + 54 + h + 29, u'la raison est écrite DANS le nœud : « ce moteur rastérise »', ORANGE, 10)

# ── 4 · LE NŒUD SURVOLÉ ──────────────────────────────────────────────────────
X4 = 690
s += lab(X4, R2, '4', u'LE NŒUD SURVOLÉ', u'le FILET s’éclaircit — jamais le corps')
n, h = noeud(X4, R2 + 54, 250, RANGS, u'Multiplier', u'Maths', CAT['outil'])
s += n
s += tt(X4, R2 + 44, u'au repos — filet #33333C', TXT2, 11, '600')
n, h = noeud(X4 + 280, R2 + 54, 250, RANGS, u'Multiplier', u'Maths', CAT['outil'], filet=None)
# on redessine le contour eclairci par-dessus : c est le SEUL changement
s += n
s += '<rect x="%s" y="%s" width="250" height="%s" fill="none" stroke="#5A5A68" stroke-width="1"/>\n' % (X4 + 280, R2 + 54, h)
s += tt(X4 + 280, R2 + 44, u'SURVOLÉ — filet #5A5A68', u'#EEF2F6', 11, '600')
s += tt(X4, R2 + 54 + h + 16, u'un signal FAIBLE suffit, et c’est un argument : le survol est transitoire et attaché au curseur.', TXT3, 10)
s += tt(X4, R2 + 54 + h + 29, u'L’utilisateur sait déjà où est sa souris. C’est la SÉLECTION qui doit survivre au déplacement du curseur.', TXT3, 10)

# ── 5 · LES TROIS GRIS ───────────────────────────────────────────────────────
R3 = 870
s += lab(34, R3, '5', u'LES TROIS GRIS, CÔTE À CÔTE — la collision qui n’en était pas une',
         u'trois causes, trois responsables, trois signaux distincts. Les mettre côte à côte est la SEULE façon de le prouver.')

TRI = [(u'INCONNU', u'l’ÉDITEUR ne le connaît pas', u'rayures d’EN-TÊTE'),
       (u'INDISPONIBLE', u'le MOTEUR ne peut pas', u'hachures de CORPS'),
       (u'DÉSACTIVÉ', u'l’UTILISATEUR l’a éteint', u'fil TRAVERSANT en pointillé')]
for i, (nom, qui, signal) in enumerate(TRI):
    x = 34 + i * 430
    s += tt(x, R3 + 44, nom, u'#EEF2F6', 12, '600')
    s += tt(x, R3 + 59, qui, TXT3, 10)
    if i == 0:
        n, h = noeud(x, R3 + 70, 250, RANGS, u'nœud.inconnu', u'?', CAT['outil'], inconnu=True)
    elif i == 1:
        n, h = noeud(x, R3 + 70, 250, RANGS, u'Light Path', u'Entrée', CAT['entree'], hachure=True, opacite=0.72)
    else:
        n, h = noeud(x, R3 + 70, 250, RANGS, u'Multiplier', u'Maths', CAT['outil'], opacite=0.45)
    s += n
    if i == 2:
        s += fil(x - 62, R3 + 70 + 45, x - 2, R3 + 70 + 45, FAM['nombre'], 2, 'pointille')
        s += fil(x + 252, R3 + 70 + 45, x + 312, R3 + 70 + 45, FAM['nombre'], 2, 'pointille')
    s += tt(x, R3 + 70 + h + 18, signal, ORANGE, 10.5, '600')

s += tt(34, R3 + 70 + h + 44, u'⚠ Je craignais une collision : les trois sont gris. Il n’y en a pas — le signal n’est pas la couleur, c’est L’ENDROIT.', TXT3, 10.5)
s += tt(34, R3 + 70 + h + 58, u'En-tête, corps, ou fil : trois zones différentes, donc trois messages lisibles ensemble. Un seul gris pour les trois rendrait', TXT3, 10.5)
s += tt(34, R3 + 70 + h + 72, u'l’éditeur inutilisable exactement au moment où l’utilisateur a besoin d’aide.', TXT3, 10.5)

c3, h3 = cartouche(34, R3 + 70 + h + 92, 1300, [
    u'✅ DÉCIDÉ 23/08 · D6 (indisponible) et D7 (survolé) sont confirmés. Ils étaient marqués PROPOSÉ aux § 8.4 et 11.4,',
    u'   pas DÉCIDÉ — l’inventaire les annonçait « non tranchés » alors que la spécification les avait déjà instruits.',
    u'📌 C’est cette divergence SILENCIEUSE qui a fait écrire verifie_coherence.py : chaque ligne de l’inventaire renvoie',
    u'   désormais au paragraphe qui doit la trancher, et le contrôle refuse qu’un « ouvert » pointe vers un « décidé ».',
], u'd’où viennent ces trois-là')
s += c3

s += tt(34, H - 50, u'Fond #121212 · corps #212121 · filet #33333C, survol #5A5A68 · hachures 45° #3A3A44 · orange Rihen #F79A28', TXT3, 10)
s += tt(34, H - 26, u'⚠ PLANCHE D’ÉTUDE — les prises y sont dessinées à environ 2,1 × leur échelle relative pour rester lisibles. Les RATIOS de la spécification font foi, jamais les pixels de cette planche.', TXT3, 10)

ecrire('planche_06_decisions.svg', s)
rendre('planche_06_decisions', W, H)
