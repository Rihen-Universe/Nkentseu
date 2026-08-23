# -*- coding: utf-8 -*-
u"""Planche 08 -- EXECUTION et VALEUR : la cohabitation, et le franchissement.

POURQUOI ELLE EXISTE
====================
Les sept premieres planches decrivent un graphe de MATIERE : un flot pur, ou
le resultat ne depend que des entrees. Le blueprint est l'autre monde -- celui
ou « ouvrir la porte puis jouer un son » n'est pas la meme chose que l'inverse.
Les deux doivent tenir dans le MEME editeur, et jusqu'ici rien ne disait
comment.

CE QU'ELLE TRANCHE, ET SUR QUEL ARGUMENT
========================================
La question n'est pas « quels domaines VEUT-ON avec des fils d'execution »
mais « lesquels en ONT BESOIN ». Le critere est mesurable :

  Un domaine a besoin de fils d'execution si, et seulement si, son resultat
  depend de l'ORDRE et des EFFETS DE BORD.

  - matiere, texture, modelisation procedurale : le resultat est une FONCTION
    des entrees. Il n'y a aucun ordre a enoncer -- le tri topologique en
    fournit un, et n'importe quel autre ordre valide donne le meme resultat.
    Les Geometry Nodes de Blender n'ont aucun fil d'execution, et ce n'est pas
    un oubli.
  - gameplay, scripting : « ouvrir la porte » puis « jouer un son » n'est pas
    « jouer un son » puis « ouvrir la porte ». L'ordre EST l'information.

Donc : gameplay et scripting OUI. Texturing et modelisation procedurale NON.
Et ce n'est pas un avis -- c'est la lecture de NkMatGraphCheck qui l'a
confirme pour la matiere : le graphe de materiau n'a AUCUN type d'execution,
pas meme declare.

CE QU'ELLE MESURE, ET QUI EST UN PROBLEME
=========================================
🔴 `NkNodeGraph` NE PEUT PAS representer un graphe d'execution aujourd'hui, et
les DEUX moities de la regle d'arite sont contredites. Releve dans
`src/NKGraph/NkNodeGraph.h` :

  - `Connect()` : « Une ENTREE n'accepte qu'UNE source : brancher une seconde
    REMPLACE la premiere. » Et `IncomingOf(n, socketIndex)` rend UN lien.
    -> Or une entree d'EXECUTION doit en accepter PLUSIEURS : dix chemins
       peuvent mener au meme noeud. Le second branchement DEBRANCHERAIT le
       premier, en silence.
  - rien ne limite le nombre de liens partant d'une SORTIE.
    -> Or une sortie d'EXECUTION doit n'en avoir QU'UN : une instruction n'a
       qu'une suite. Deux suites, c'est deux programmes.

C'est la regle d'arite INVERSEE du catalogue, et le coeur applique
uniformement celle de la donnee. Ce n'est pas un defaut de dessin : c'est le
modele qui manque d'un axe.

⚠️ ET UNE QUESTION QUE JE NE TRANCHE PAS, parce qu'elle demande une mesure que
je n'ai pas faite : `TopoSort` / `HasCycle` REFUSENT tout cycle. Un corps de
boucle qui se reboucle explicitement -- ce qu'un blueprint permet -- serait
donc refuse. Est-ce voulu (le corps de boucle se referme par la semantique du
noeud, pas par un fil) ou est-ce un mur ? Il faut le mesurer, pas le supposer.
"""
import sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from gen import *

W, H = 1820, 1640

# Ce que cette planche presente encore comme ouvert, en forme lisible par
# verifie_planches.py. Voir p01.py pour le pourquoi.
OUVERTS_DECLARES = [
    ('hors', '18.4', u'arite d execution : le modele applique celle de la donnee'),
    ('hors', '18.5', u'un corps de boucle qui se reboucle : refuse par HasCycle ?'),
]

s = head(W, H, u'Planche 08 — EXÉCUTION et VALEUR : la cohabitation, et le franchissement',
         u'quels domaines ont besoin de fils d’exécution · comment les deux familles tiennent dans un même nœud · comment un blueprint et un graphe de matière se parlent SANS se toucher')

VERT = '#4E9A5A'
ROUGE_D = '#E06C6C'


def lab(x, y, n, titre, note=u''):
    o = tt(x, y, u'%s · %s' % (n, titre), ORANGE, 12.5, '600')
    if note:
        o += tt(x, y + 16, note, TXT3, 10)
    return o


# ══════════════════════════════════════════════════════ 1 · UN BLUEPRINT REEL
R1 = 108
s += lab(34, R1, '1', u'UN BLUEPRINT RÉEL — les deux familles de fil dans le même graphe',
         u'l’exécution passe AU-DESSUS du filet et se lit de gauche à droite · la valeur passe EN DESSOUS et n’a pas d’ordre')

n1, h1 = noeud(44, R1 + 40, 250, [
    {'lab': u'Ouverte', 'coul': FAM['exec'], 'plein': True, 'sortie': True, 'exec': True},
    {'lab': u'Acteur', 'coul': FAM['ref'], 'plein': False, 'sortie': True, 'glyphe': 'obj'},
], u'Sur porte ouverte', u'Événement', CAT['flot'], exec_=True, aide=False)
s += n1
s += tt(44, R1 + 40 + h1 + 16, u'un ÉVÉNEMENT : aucune entrée d’exécution —', TXT3, 10)
s += tt(44, R1 + 40 + h1 + 30, u'c’est LUI qui démarre le fil.', TXT3, 10)

n2, h2 = noeud(374, R1 + 40, 260, [
    {'lab': u'Entrer', 'coul': FAM['exec'], 'plein': True, 'exec': True},
    {'lab': u'Terminé', 'coul': FAM['exec'], 'plein': True, 'sortie': True, 'exec': True},
    {'note': ''},
    {'lab': u'Cible', 'coul': FAM['ref'], 'plein': True, 'branchee': True, 'glyphe': 'obj'},
    {'lab': u'Durée', 'coul': FAM['nombre'], 'plein': False, 'val': '0.400', 'glyphe': '1.0'},
], u'Jouer l’animation', u'Action', CAT['flot'], exec_=True, aide=False)
s += n2

n3, h3 = noeud(714, R1 + 40, 260, [
    {'lab': u'Entrer', 'coul': FAM['exec'], 'plein': True, 'exec': True},
    {'lab': u'Terminé', 'coul': FAM['exec'], 'plein': True, 'sortie': True, 'exec': True},
    {'note': ''},
    {'lab': u'Son', 'coul': FAM['ref'], 'plein': False, 'ctrl': 'lecture', 'val': u'grincement', 'glyphe': 'obj'},
    {'lab': u'Volume', 'coul': FAM['nombre'], 'plein': True, 'branchee': True, 'glyphe': '1.0'},
], u'Jouer un son', u'Action', CAT['flot'], exec_=True, aide=False)
s += n3

# la source de valeur, POSEE PLUS BAS : elle n'a aucune place dans la
# chaine d'execution, et le dessin doit le montrer.
n4, h4 = noeud(374, R1 + 250, 260, [
    {'lab': u'A', 'coul': FAM['nombre'], 'plein': False, 'val': '0.800', 'glyphe': '1.0'},
    {'lab': u'B', 'coul': FAM['nombre'], 'plein': False, 'val': '0.500', 'glyphe': '1.0'},
    {'lab': u'Résultat', 'coul': FAM['nombre'], 'plein': False, 'sortie': True, 'glyphe': '1.0'},
], u'Multiplier', u'Maths', CAT['outil'], aide=False)
s += n4

# les fils d'EXECUTION : epais, orange, entre les prises d'en-tete
YE = R1 + 40 + 30 + 6 + 12
s += fil(294, YE, 374, YE, ORANGE, 3.5)
s += fil(634, YE, 714, YE, ORANGE, 3.5)
# le fil de VALEUR : fin, couleur du type, et il monte d'en dessous
s += fil(634, R1 + 250 + 30 + 6 + 12 + 48, 714, YE + 96, FAM['nombre'], 2)

s += tt(374, R1 + 250 + h4 + 16, u'⚠ ce nœud n’est dans AUCUNE chaîne d’exécution, et pourtant il', ORANGE, 10)
s += tt(374, R1 + 250 + h4 + 30, u'est évalué : une valeur est calculée QUAND ON LA DEMANDE.', TXT2, 10)
s += tt(374, R1 + 250 + h4 + 44, u'C’est la différence de nature entre les deux familles, et c’est', TXT2, 10)
s += tt(374, R1 + 250 + h4 + 58, u'la seule chose que ce panneau doit faire comprendre.', TXT2, 10)

c1, hc1 = cartouche(1000, R1 + 40, 786, [
    u'▸ L’EXÉCUTION est un CHEMIN : elle a un début (l’événement), un ordre, et un seul point à la fois. Elle répond à « quand ».',
    u'▸ La VALEUR est une DÉPENDANCE : elle n’a ni début ni ordre, seulement des producteurs et des consommateurs. Elle répond à « avec quoi ».',
    u'▸ D’où le dessin : l’exécution est posée SUR L’EN-TÊTE, au-dessus du filet, parce qu’elle concerne le nœud ENTIER — pas une de ses rangées.',
    u'   La valeur est posée sur ses rangées, parce que c’est la rangée qu’elle alimente. Le filet orange n’est pas un ornement : c’est la frontière.',
    u'▸ ⚠ Un nœud d’exécution garde ses entrées de valeur. L’inverse est faux : un nœud de calcul n’a JAMAIS de prise d’exécution.',
    u'   C’est ce qui permet aux deux mondes de partager le même catalogue de nœuds de calcul, sans en dupliquer un seul.',
], u'ce que les deux familles sont, et pourquoi le dessin les sépare ainsi')
s += c1

# ══════════════════════════════════════ 2 · QUELS DOMAINES EN ONT BESOIN
R2 = 560
s += lab(34, R2, '2', u'QUELS DOMAINES ONT BESOIN DE FILS D’EXÉCUTION — et le critère qui décide',
         u'la question n’est pas ce qu’on veut, c’est ce dont le domaine a BESOIN · le critère se vérifie domaine par domaine, sans discuter')

DOM = [
    (u'gameplay', True, u'« ouvrir la porte » puis « jouer un son » ≠ l’inverse'),
    (u'scripting', True, u'écrire un fichier, appeler un service : effets de bord'),
    (u'matériau / texture', False, u'MESURÉ : le modèle n’a aucun type d’exécution (§17)'),
    (u'modélisation procédurale', False, u'les Geometry Nodes de Blender n’en ont aucun'),
    (u'animation (graphe d’états)', None, u'⚠ ni l’un ni l’autre : ses arcs sont des TRANSITIONS'),
]
yy = R2 + 44
for (nom, oui, pourquoi) in DOM:
    coul = VERT if oui is True else (ROUGE_D if oui is False else ORANGE)
    marque = u'OUI' if oui is True else (u'NON' if oui is False else u'À PART')
    s += '<rect x="44" y="%d" width="66" height="19" rx="3" fill="%s" opacity="0.20"/>\n' % (yy - 14, coul)
    s += tt(77, yy, marque, coul, 10, '600', 'middle')
    s += tt(122, yy, nom, TXT, 11.5, '600')
    s += tt(330, yy, pourquoi, TXT2, 10.5)
    yy += 28

s += tt(44, yy + 10, u'LE CRITÈRE — et il se vérifie, il ne se discute pas :', ORANGE, 11.5, '600')
s += tt(44, yy + 30, u'un domaine a besoin de fils d’exécution si, et seulement si, son résultat dépend de l’ORDRE et des EFFETS DE BORD.', TXT, 11)
s += tt(44, yy + 48, u'Quand le résultat est une FONCTION des entrées, il n’y a aucun ordre à énoncer : le tri topologique en fournit un, et', TXT2, 10.5)
s += tt(44, yy + 64, u'n’importe quel autre ordre valide donne le même résultat. Dessiner un fil d’exécution y ajouterait une contrainte FAUSSE.', TXT2, 10.5)

c2, hc2 = cartouche(1000, R2 + 40, 786, [
    u'▸ ⚠ LE GRAPHE D’ÉTATS D’ANIMATION N’EST NI L’UN NI L’AUTRE, et le ranger dans « exécution » serait une erreur coûteuse :',
    u'   ses arcs ne transportent rien et ne déclenchent rien — ils portent une CONDITION et une durée de fondu. Le graphe ne « s’exécute »',
    u'   pas de gauche à droite : il a un état COURANT, et il y reste. C’est une troisième famille, et elle mérite sa propre planche.',
    u'▸ ✅ CE QUE CE DÉCOUPAGE OFFRE, et c’est la vraie raison de le poser maintenant : un graphe SANS exécution est APPELABLE COMME',
    u'   UNE FONCTION. Pur, sans effet de bord, il peut être évalué à la demande, mis en cache, appelé depuis un blueprint (panneau 5).',
    u'   Un graphe AVEC exécution ne le peut pas : l’appeler, c’est le LANCER. La frontière du panneau 2 est donc aussi celle du panneau 5.',
], u'le cas qui ne rentre dans aucune des deux cases — et ce que la frontière offre')
s += c2

# ══════════════════════════════════ 3 · CE QUE LE MODELE NE PERMET PAS
R3 = 900
s += lab(34, R3, '3', u'🔴 CE QUE LE MODÈLE NE PERMET PAS AUJOURD’HUI — les DEUX moitiés de la règle d’arité',
         u'relevé dans src/NKGraph/NkNodeGraph.h · ce n’est pas un défaut de dessin, c’est un axe qui manque au modèle')

# cas a : plusieurs chemins vers une meme entree d execution
s += tt(44, R3 + 46, u'a · PLUSIEURS chemins doivent pouvoir mener au MÊME nœud', TXT, 11.5, '600')
na, ha = noeud(44, R3 + 60, 190, [
    {'lab': u'Terminé', 'coul': FAM['exec'], 'plein': True, 'sortie': True, 'exec': True},
], u'Si — vrai', None, CAT['flot'], exec_=True, aide=False, marque=False)
s += na
nb, hb = noeud(44, R3 + 140, 190, [
    {'lab': u'Terminé', 'coul': FAM['exec'], 'plein': True, 'sortie': True, 'exec': True},
], u'Si — faux', None, CAT['flot'], exec_=True, aide=False, marque=False)
s += nb
nc, hc = noeud(330, R3 + 100, 200, [
    {'lab': u'Entrer', 'coul': FAM['exec'], 'plein': True, 'exec': True},
], u'Fermer la porte', None, CAT['flot'], exec_=True, aide=False, marque=False)
s += nc
s += fil(234, R3 + 60 + 21 + 6 + 12, 330, R3 + 100 + 21 + 6 + 12, ORANGE, 3.5)
s += fil(234, R3 + 140 + 21 + 6 + 12, 330, R3 + 100 + 21 + 6 + 12, ORANGE, 3.5)
s += tt(44, R3 + 210, u'❌ Connect() : « une ENTRÉE n’accepte qu’UNE source :', ROUGE_D, 10.5, '600')
s += tt(44, R3 + 224, u'brancher une seconde REMPLACE la première ». Le second', TXT2, 10)
s += tt(44, R3 + 238, u'fil DÉBRANCHERAIT le premier, en silence.', TXT2, 10)

# cas b : une sortie d execution ne doit avoir qu une suite
s += tt(600, R3 + 46, u'b · UNE sortie d’exécution n’a qu’UNE suite', TXT, 11.5, '600')
nd, hd = noeud(600, R3 + 100, 200, [
    {'lab': u'Terminé', 'coul': FAM['exec'], 'plein': True, 'sortie': True, 'exec': True},
], u'Ouvrir la porte', None, CAT['flot'], exec_=True, aide=False, marque=False)
s += nd
ne_, he = noeud(880, R3 + 60, 190, [
    {'lab': u'Entrer', 'coul': FAM['exec'], 'plein': True, 'exec': True},
], u'Jouer un son', None, CAT['flot'], exec_=True, aide=False, marque=False)
s += ne_
nf, hf = noeud(880, R3 + 140, 190, [
    {'lab': u'Entrer', 'coul': FAM['exec'], 'plein': True, 'exec': True},
], u'Compter un point', None, CAT['flot'], exec_=True, aide=False, marque=False)
s += nf
s += fil(806, R3 + 100 + 21 + 6 + 12, 880, R3 + 60 + 21 + 6 + 12, ORANGE, 3.5)
s += fil(806, R3 + 100 + 21 + 6 + 12, 880, R3 + 140 + 21 + 6 + 12, ROUGE_D, 3.5)
s += tt(600, R3 + 210, u'❌ Rien ne limite le nombre de liens partant d’une SORTIE.', ROUGE_D, 10.5, '600')
s += tt(600, R3 + 224, u'Deux suites, ce sont deux programmes — et rien ne dit lequel', TXT2, 10)
s += tt(600, R3 + 238, u'passe en premier. Il faut REFUSER le second, en le nommant.', TXT2, 10)

c3, hc3 = cartouche(1120, R3 + 40, 666, [
    u'▸ Le cœur applique UNIFORMÉMENT l’arité de la donnée : une entrée, une source ; une sortie, autant de liens qu’on veut.',
    u'▸ Pour l’exécution il faut exactement l’INVERSE, et le catalogue l’écrit déjà : « les arités sont INVERSÉES entre les deux familles ».',
    u'▸ 📌 Ce qui manque n’est pas une exception, c’est un AXE : le socket doit porter sa famille. Une seule valeur, deux lignes dans',
    u'   Connect() — et le refus doit SE NOMMER, comme les six raisons de NkLinkError le font déjà (ExecOutputAlreadyBound).',
    u'▸ ⚠ NON MESURÉ, et je refuse de le supposer : TopoSort et HasCycle REFUSENT tout cycle. Un corps de boucle qui se reboucle',
    u'   explicitement serait donc refusé. Est-ce voulu — le corps se referme par la sémantique du nœud, pas par un fil — ou est-ce un mur ?',
], u'ce qui manque au modèle, et la seule chose que je n’ai pas mesurée')
s += c3

# ══════════════════════════ 4 et 5 · LE FRANCHISSEMENT, DEUX MÉCANISMES
R4 = 1210
s += lab(34, R4, '4', u'LE FRANCHISSEMENT (1) — par PARAMÈTRE EXPOSÉ : on POUSSE une valeur dans un graphe vivant',
         u'aucun fil ne traverse · les deux graphes ne partagent jamais un canevas · le lien se fait par un NOM')

ng, hg = noeud(44, R4 + 40, 290, [
    {'lab': u'Entrer', 'coul': FAM['exec'], 'plein': True, 'exec': True},
    {'lab': u'Terminé', 'coul': FAM['exec'], 'plein': True, 'sortie': True, 'exec': True},
    {'note': ''},
    {'lab': u'Matériau', 'coul': FAM['ref'], 'plein': False, 'ctrl': 'lecture', 'val': u'mur_pierre', 'glyphe': 'obj'},
    {'lab': u'Nom', 'coul': FAM['texte'], 'plein': True, 'ctrl': 'lecture', 'val': u'« Rugosité »', 'glyphe': 'txt'},
    {'lab': u'Valeur', 'coul': FAM['nombre'], 'plein': False, 'val': '0.850', 'glyphe': '1.0'},
], u'Définir un paramètre', u'Action · blueprint', CAT['flot'], exec_=True, aide=False)
s += ng

nh, hh2 = noeud(470, R4 + 40, 250, [
    {'lab': u'Valeur', 'coul': FAM['nombre'], 'plein': False, 'sortie': True, 'glyphe': '1.0'},
], u'Paramètre « Rugosité »', u'Source · matériau', CAT['entree'], aide=False)
s += nh
ni, hi = noeud(770, R4 + 40, 230, [
    {'lab': u'Rugosité', 'coul': FAM['nombre'], 'plein': True, 'branchee': True, 'glyphe': '1.0'},
    {'lab': u'Surface', 'coul': FAM['appar'], 'plein': True, 'sortie': True, 'glyphe': 'SH'},
], u'Principled', u'Surface', CAT['surface'], aide=False)
s += ni
s += fil(720, R4 + 40 + 30 + 6 + 12, 770, R4 + 40 + 30 + 6 + 12, FAM['nombre'], 2)

# la FRONTIERE, dessinee : deux canevas, pas un
s += ('<rect x="404" y="%d" width="2" height="150" fill="%s" opacity="0.45"/>\n'
      % (R4 + 30, ORANGE))
s += tt(414, R4 + 44, u'FRONTIÈRE — deux documents', ORANGE, 10, '600')
s += tt(414, R4 + 58, u'aucun fil ne la traverse', TXT3, 10)
s += tt(44, R4 + 40 + hg + 16, u'le blueprint ne CONNAÎT pas le graphe de matière : il nomme un paramètre.', TXT2, 10)
s += tt(44, R4 + 40 + hg + 30, u'⚠ le nom est donc une CLÉ publique — le renommer casse le blueprint, en silence,', ORANGE, 10)
s += tt(44, R4 + 40 + hg + 44, u'et c’est le seul coût réel de ce mécanisme. Il faut le refuser ou le répercuter.', TXT2, 10)

s += lab(1060, R4, '5', u'LE FRANCHISSEMENT (2) — par APPEL DE GRAPHE, comme une FONCTION',
         u'le graphe appelé devient un NŒUD · ses prises sont celles de son interface · il DOIT être pur, et le panneau 2 dit lequel l’est')

nj, hj = noeud(1070, R4 + 40, 300, [
    {'lab': u'Échelle', 'coul': FAM['nombre'], 'plein': False, 'val': '4.000', 'glyphe': '1.0'},
    {'lab': u'Détail', 'coul': FAM['nombre'], 'plein': True, 'branchee': True, 'glyphe': '1.0'},
    {'lab': u'Facteur', 'coul': FAM['nombre'], 'plein': False, 'sortie': True, 'glyphe': '1.0'},
], u'Bruit de surface', u'Graphe appelé ③', CAT['variable'], aide=False)
s += nj
s += tt(1070, R4 + 40 + hj + 16, u'✅ CE MÉCANISME EXISTE DÉJÀ DANS LE MODÈLE, et je ne l’invente pas :', u'#7FB77E', 10.5, '600')
s += tt(1070, R4 + 40 + hj + 30, u'NkNode porte un champ subgraph — « nom du graphe instancié dans le', TXT2, 10)
s += tt(1070, R4 + 40 + hj + 44, u'document », renseigné uniquement sur un nœud de type NK_NODE_INSTANCE.', TXT2, 10)
s += tt(1070, R4 + 40 + hj + 62, u'⚠ Et ③ n’est pas un ornement : on entre par UNE instance mais on édite la', ORANGE, 10)
s += tt(1070, R4 + 40 + hj + 76, u'DÉFINITION, qui est PARTAGÉE. C’est le compteur du fil d’Ariane (planche 06).', TXT2, 10)

c4, hc4 = cartouche(34, R4 + 250, 1010, [
    u'▸ LES DEUX MÉCANISMES NE SONT PAS DES VARIANTES DU MÊME, et les confondre coûterait cher : le PARAMÈTRE pousse une valeur dans un graphe',
    u'   qui VIT et se recalcule tout seul ; l’APPEL évalue un graphe à la demande et récupère son résultat. L’un écrit, l’autre lit.',
    u'▸ Le choix ne se discute pas non plus, il se déduit : le blueprint veut-il CHANGER l’apparence d’un objet du monde (paramètre), ou',
    u'   RÉUTILISER un calcul (appel) ? La première question a une réponse dans le monde, la seconde dans le graphe.',
    u'▸ 🔴 L’APPEL EXIGE QUE LE GRAPHE APPELÉ SOIT PUR — sans exécution, sans effet de bord — sinon « l’appeler » voudrait dire « le lancer »,',
    u'   et il faudrait décider quand. C’est exactement la frontière du panneau 2 : ce qui n’a pas de fil d’exécution est appelable, le reste non.',
    u'▸ ⚠ Le sens inverse — un graphe de matière qui appellerait un blueprint — est REFUSÉ, et il faut le dire au moment du branchement :',
    u'   un matériau se recalcule à chaque image et à chaque pixel. Y loger un effet de bord, c’est le déclencher un million de fois.',
], u'quand employer l’un plutôt que l’autre — et le sens qu’il faut refuser')
s += c4

s += tt(34, H - 44, u'fil d’exécution : 3,5 px, TOUJOURS orange, jamais coloré par un type · fil de valeur : 2 px, couleur du type · prise d’exécution sur l’EN-TÊTE, prise de donnée sur sa rangée', TXT3, 10)
s += tt(34, H - 26, u'⚠ PLANCHE D’ÉTUDE — les prises y sont dessinées à environ 2,1 × leur échelle relative pour rester lisibles. Les RATIOS de la spécification font foi, jamais les pixels de cette planche.', TXT3, 10)

ecrire('planche_08_execution.svg', s)
rendre('planche_08_execution', W, H)
