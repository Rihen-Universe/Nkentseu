# -*- coding: utf-8 -*-
# Planche 07 -- les six elements tranches le 23/08 apres la planche 06.
#
# Elle existe pour UNE raison : deux des six decisions mettent DEUX OBJETS AU
# MEME ENDROIT, et ca ne se demontre pas par une phrase. Il faut les poser cote a
# cote et regarder si on les confond. Le reste de la planche montre les quatre
# decisions ou la reponse a ete « ne rien ajouter » -- et pour celles-la l image
# sert a prouver que la marque existante SUFFIT.
#
# ⚠️ REFAITE le 23/08 sur deux corrections de Rodolf :
#   - le POINTILLE DU LASSO est ADOPTE (le panneau 1 ne montre plus un refus,
#     mais les trois axes qui separent deux rectangles pointilles) ;
#   - le COMMENTAIRE est VISIBLE PAR DEFAUT (le panneau 3 legendait « au survol
#     seulement », ce qui se lisait comme si le commentaire, et non le fond,
#     apparaissait au survol).
import sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from gen import *

W, H = 1760, 1050
s = head(W, H, u'Planche 07 — le lasso, le menu, la recherche vide, la source, le puits, le commentaire',
         u'six éléments tranchés le 23/08 — deux corrections de Rodolf intégrées : le pointillé du lasso est ADOPTÉ, et le commentaire est VISIBLE PAR DÉFAUT')


def lab(x, y, n, titre, note=u''):
    return tt(x, y, u'%s · %s' % (n, titre), ORANGE, 12.5, '600') + tt(x, y + 16, note, TXT3, 10)


def cadre(x, y, w=230, h=130):
    """Le cadre valide par Rodolf le 22/08 : filet plein dehors, POINTILLE dedans."""
    t = u'#4C8C4A'
    return (('<rect x="%d" y="%d" width="%d" height="%d" rx="6" fill="%s" opacity="0.08"/>\n'
             '<rect x="%d" y="%d" width="%d" height="%d" rx="6" fill="none" stroke="%s" stroke-width="1.5"/>\n'
             '<rect x="%d" y="%d" width="%d" height="%d" rx="4" fill="none" stroke="%s" stroke-width="1" '
             'stroke-dasharray="4 4" opacity="0.7"/>\n'
             '<rect x="%d" y="%d" width="%d" height="20" rx="6" fill="%s"/>\n')
            % (x, y, w, h, t, x, y, w, h, t, x + 8, y + 8, w - 16, h - 16, t, x, y, w, t)
            + tt(x + 8, y + 14, u'Éclairage', u'#10240F', 12.5, '600')
            + tt(x + w - 8, y + 14, u'2 nodes', u'#10240F', 11, None, 'end'))


# ── 1 · LASSO ET CADRE : DEUX RECTANGLES POINTILLES, ET CE QUI LES SEPARE ────
# Rodolf, 23/08 : « la planche 07 dit que le pointille est refuse, pourtant c est
# pas le cas, on l adopte. » Ce panneau ne montre donc plus un REFUS mais une
# DISTINCTION -- et la collision de GESTES que j avais invoquee n existait pas :
# le 12.5 cree le cadre PAR LE MENU, autour de la selection.
R1 = 104
s += lab(34, R1, '1', u'LE LASSO ET LE CADRE — deux rectangles POINTILLÉS, et les trois axes qui les distinguent',
         u'le pointillé est ADOPTÉ (Rodolf, 23/08) ; ce qui les sépare n’est pas le motif — c’est le NOMBRE DE FILETS, le bandeau, et la couleur')


def lasso(x, y, w=230, h=130):
    """Le lasso : UN seul filet, pointille, orange, et un remplissage a 8 %."""
    return (('<rect x="%d" y="%d" width="%d" height="%d" rx="4" fill="%s" opacity="0.08"/>\n'
             '<rect x="%d" y="%d" width="%d" height="%d" rx="4" fill="none" stroke="%s" '
             'stroke-width="1.5" stroke-dasharray="4 4"/>\n')
            % (x, y, w, h, ORANGE, x, y, w, h, ORANGE))


# -- a gauche : les deux objets, cote a cote, tous deux pointilles -------------
s += tt(34, R1 + 46, u'LES DEUX PORTENT DU POINTILLÉ — et c’est très bien', u'#EEF2F6', 11.5, '600')

s += cadre(34, R1 + 66)
s += tt(34, R1 + 214, u'un CADRE — permanent', u'#EEF2F6', 11, '600')
s += tt(34, R1 + 229, u'DEUX filets : plein dehors, pointillé EN RETRAIT', TXT2, 10)
s += tt(34, R1 + 243, u'+ un bandeau de titre, toujours', TXT2, 10)

s += lasso(300, R1 + 66)
s += tt(300, R1 + 214, u'un LASSO — transitoire', ORANGE, 11, '600')
s += tt(300, R1 + 229, u'UN filet, pointillé, SUR LE BORD', TXT2, 10)
s += tt(300, R1 + 243, u'aucun bandeau — et il disparaît au relâchement', TXT2, 10)

# -- a droite : les trois axes, du plus solide au plus fragile -----------------
XA = 600
s += tt(XA, R1 + 46, u'LES TROIS AXES, DU PLUS SOLIDE AU PLUS FRAGILE', u'#EEF2F6', 11.5, '600')

# axe 1 : un filet contre deux -- le zoom sur le bord
s += tt(XA, R1 + 70, u'1 · le NOMBRE DE FILETS', u'#7FB77E', 11, '600')
s += tt(XA, R1 + 85, u'ne dépend d’aucun choix de l’utilisateur', TXT3, 9.5)
t = u'#4C8C4A'
s += '<rect x="%d" y="%d" width="150" height="54" rx="4" fill="%s" opacity="0.08"/>\n' % (XA, R1 + 94, t)
s += '<rect x="%d" y="%d" width="150" height="54" rx="4" fill="none" stroke="%s" stroke-width="1.5"/>\n' % (XA, R1 + 94, t)
s += ('<rect x="%d" y="%d" width="134" height="38" rx="3" fill="none" stroke="%s" stroke-width="1" '
      'stroke-dasharray="4 4" opacity="0.7"/>\n') % (XA + 8, R1 + 102, t)
s += tt(XA, R1 + 164, u'cadre : le pointillé est DEDANS', TXT2, 10)
s += '<rect x="%d" y="%d" width="150" height="54" rx="4" fill="%s" opacity="0.08"/>\n' % (XA + 176, R1 + 94, ORANGE)
s += ('<rect x="%d" y="%d" width="150" height="54" rx="4" fill="none" stroke="%s" stroke-width="1.5" '
      'stroke-dasharray="4 4"/>\n') % (XA + 176, R1 + 94, ORANGE)
s += tt(XA + 176, R1 + 164, u'lasso : le pointillé est LE BORD', ORANGE, 10, '600')
s += tt(XA, R1 + 180, u'✅ un pointillé SUR LE BORD ne peut être qu’un lasso — le cadre n’en a jamais sur le sien', u'#7FB77E', 10)

# axe 2 : le bandeau
s += tt(XA, R1 + 206, u'2 · le BANDEAU', u'#7FB77E', 11, '600')
s += tt(XA, R1 + 221, u'gratuit : il existe de toute façon, et il occupe le bord HAUT — là où l’œil arrive', TXT2, 10)
s += tt(XA, R1 + 236, u'le titre est obligatoire depuis l’option A (22/08) : un cadre en a un dès sa création', TXT3, 9.5)

# axe 3 : la couleur, et sa faiblesse dite
s += tt(XA, R1 + 258, u'3 · l’ORANGE RÉSERVÉ', u'#E0A96C', 11, '600')
s += tt(XA, R1 + 273, u'⚠ le plus FAIBLE, et il faut dire pourquoi : la teinte du cadre est CHOISIE par', ORANGE, 10)
s += tt(XA, R1 + 287, u'l’utilisateur. C’est une contrainte sur la PALETTE OFFERTE, pas une propriété du', TXT2, 10)
s += tt(XA, R1 + 301, u'dessin — elle tombe le jour où quelqu’un laisse choisir n’importe quelle teinte.', TXT2, 10)

c1, h1 = cartouche(34, R1 + 322, 1062, [
    u'❌ CE QUE CETTE PLANCHE DISAIT, ET QUI ÉTAIT FAUX : « tirer un rectangle sur le fond est aussi le geste qui crée un cadre ».',
    u'   Mon propre § 12.5 dit le contraire — le menu du fond porte « créer un cadre AUTOUR DE LA SÉLECTION ». Le cadre ne se tire pas.',
    u'   📌 J’avais mesuré une collision de FORMES et j’en ai déduit une collision de GESTES sans vérifier le geste. Les deux ne se rencontrent jamais.',
    u'⚠ LE POINTILLÉ EST EMPLOYÉ SIX FOIS : dictionnaire (sous une PRISE) · fil tiré (un FIL) · nœud désactivé (le fil TRAVERSANT) · prise',
    u'   convertible (un HALO) · cadre (un RECTANGLE) · lasso (un RECTANGLE). ✅ Quatre ne se gênent pas : le signal n’est pas le pointillé, c’est L’ENDROIT.',
    u'✅ Et pour les deux qui partagent l’endroit, la réponse n’est pas de retirer le motif à l’un — c’est de les séparer sur trois AUTRES axes.',
], u'ce que ce panneau disait hier, et pourquoi il a changé')
s += c1

# ── 2 · LA RECHERCHE SANS RÉSULTAT ───────────────────────────────────────────
X2 = 1140
s += lab(X2, R1, '2', u'LA RECHERCHE SANS RÉSULTAT',
         u'un résultat négatif sans son périmètre est une rumeur')

for i, bon in enumerate([False, True]):
    y = R1 + 46 + i * 152
    s += tt(X2, y, u'✅ ce qui est décidé' if bon else u'❌ la liste vide, interdite',
            u'#7FB77E' if bon else u'#E06C6C', 11.5, '600')
    s += '<rect x="%d" y="%d" width="330" height="120" rx="5" fill="#1E1E24" stroke="#3A3A44"/>\n' % (X2, y + 10)
    s += '<rect x="%d" y="%d" width="314" height="22" rx="3" fill="#141418" stroke="#33333C"/>\n' % (X2 + 8, y + 18)
    s += tt(X2 + 16, y + 33, u'🔍', TXT3, 11)
    if bon:
        s += '<rect x="%d" y="%d" width="314" height="26" rx="3" fill="%s" opacity="0.10"/>\n' % (X2 + 8, y + 46, ORANGE)
        s += tt(X2 + 16, y + 63, u'qui acceptent', TXT3, 10)
        s += '<rect x="%d" y="%d" width="98" height="15" rx="2" fill="%s" opacity="0.30"/>\n' % (X2 + 86, y + 52, FAM['nombre'])
        s += tt(X2 + 91, y + 63, u'tableau de réels', u'#EEF2F6', 9.5)
        s += tt(X2 + 306, y + 64, u'✕', ORANGE, 12, '600')
        s += tt(X2 + 16, y + 92, u'aucun nœud n’accepte', u'#EEF2F6', 11, '600')
        s += tt(X2 + 16, y + 107, u'tableau de réels', ORANGE, 11, '600')
        s += tt(X2 + 16, y + 122, u'↑ et le ✕ qui le retire est juste au-dessus', TXT3, 9.5)
    else:
        s += tt(X2 + 16, y + 78, u'(rien)', TXT3, 11)
        s += tt(X2 + 16, y + 104, u'⚠ se lit « ce nœud n’existe pas »', u'#E06C6C', 10, '600')
        s += tt(X2 + 16, y + 122, u'alors que le fait est « pas de ce TYPE »', TXT3, 9.5)

c2, h2 = cartouche(X2, R1 + 352, 330, [
    u'⚠ La vraie phrase n’est pas « ce nœud',
    u'   n’existe pas » mais « ce nœud',
    u'   n’accepte pas CE TYPE ».',
    u'✅ Le filtre s’écrit en toutes lettres,',
    u'   et son ✕ est JUSTE À CÔTÉ du refus.',
], u'la faute que ça évite')
s += c2

# ── 3 · SOURCE, PUITS, COMMENTAIRE ───────────────────────────────────────────
R2 = R1 + 322 + h1 + 44
s += lab(34, R2, '3', u'LA SOURCE, LE PUITS, LE COMMENTAIRE',
         u'les deux premiers : « ne RIEN ajouter », chacun avait déjà sa marque. Le troisième : VISIBLE PAR DÉFAUT — « un commentaire qu’il faut aller chercher n’est pas lu »')

n, _ = noeud(34, R2 + 44, 190, [{'lab': u'Couleur', 'coul': FAM['appar'], 'plein': True, 'sortie': True}],
             u'RVB', u'Entrée', FAM['appar'])
s += n
s += tt(34, R2 + 150, u'SOURCE — le côté GAUCHE est vide', u'#EEF2F6', 11, '600')
s += tt(34, R2 + 165, u'le seul trait de forme qui survit à 25 %,', TXT3, 10)
s += tt(34, R2 + 179, u'où il ne reste qu’un rectangle coloré', TXT3, 10)

n, _ = noeud(280, R2 + 44, 190, [{'lab': u'Surface', 'coul': FAM['appar'], 'plein': True},
                                 {'lab': u'Volume', 'coul': FAM['appar'], 'plein': True}],
             u'Sortie matériau', u'Sortie', FAM['appar'])
s += n
s += tt(280, R2 + 150, u'PUITS — le côté DROIT est vide', u'#EEF2F6', 11, '600')
s += tt(280, R2 + 165, u'le catalogue permettait « plus imposant » :', TXT3, 10)
s += tt(280, R2 + 179, u'refusé — une exception à la grille pour un doublon', TXT3, 10)

s += fil(534, R2 + 78, 764, R2 + 96, FAM['nombre'], 2)
s += tt(534, R2 + 58, u'ce bloc gère le cas où la texture manque', TXT3, 13)
s += tt(530, R2 + 150, u'COMMENTAIRE — PAR DÉFAUT : toujours affiché', u'#7FB77E', 11, '600')
s += tt(530, R2 + 165, u'ni corps, ni filet, ni fond : ce n’est pas un nœud —', TXT3, 10)
s += tt(530, R2 + 179, u'et il passe DEVANT le fil, qui sinon le barrerait', TXT3, 10)
s += tt(530, R2 + 193, u'✅ AUCUN geste à faire pour le lire', u'#7FB77E', 10, '600')

s += '<rect x="800" y="%d" width="292" height="26" rx="3" fill="#1A1A1A" opacity="0.6"/>\n' % (R2 + 44)
s += tt(806, R2 + 62, u'ce bloc gère le cas où la texture manque', TXT3, 13)
s += tt(806, R2 + 150, u'AU SURVOL — le FOND apparaît, pas le texte', u'#EEF2F6', 11, '600')
s += tt(806, R2 + 165, u'⚠ il ne RÉVÈLE rien : le texte était déjà là. Il dit OÙ CLIQUER.', ORANGE, 10)
s += tt(806, R2 + 179, u'signal FAIBLE volontairement — plus marqué, il donnerait au', TXT3, 10)
s += tt(806, R2 + 193, u'commentaire l’allure d’un nœud sans en-tête, ce qu’on interdit', TXT3, 10)
s += tt(530, R2 + 213, u'✅ OPTION, désactivée à l’installation : « n’afficher les commentaires qu’au survol » — pour un canevas dense qu’on veut nettoyer.', TXT2, 10.5)
s += tt(530, R2 + 228, u'⚠ C’est l’utilisateur qui l’active. Par défaut le commentaire est LÀ : un commentaire qu’il faut aller chercher n’est pas lu.', ORANGE, 10.5)

# ── 4 · LE MENU CONTEXTUEL ───────────────────────────────────────────────────
R4 = R1 + 352 + h2 + 42
s += lab(X2, R4, '4', u'LE MENU CONTEXTUEL — trois contenus',
         u'et « ajouter un nœud » rouvre LE MÊME panneau que le 2, sans filtre')

MENUS = [(u'sur le FOND', [u'ajouter un nœud', u'coller', u'cadrer tout', u'créer un cadre'], -1),
         (u'sur un NŒUD', [u'dupliquer', u'désactiver', u'grouper', u'supprimer'], -1),
         (u'sur un FIL', [u'insérer un nœud ici', u'supprimer le fil'], 0)]
for i, (ou, items, fort) in enumerate(MENUS):
    x = X2 + i * 116
    s += tt(x, R4 + 44, ou, TXT2, 10, '600')
    s += ('<rect x="%d" y="%d" width="106" height="%d" rx="4" fill="#1E1E24" stroke="#3A3A44"/>\n'
          % (x, R4 + 52, 12 + len(items) * 19))
    for j, it in enumerate(items):
        s += tt(x + 8, R4 + 71 + j * 19, it, ORANGE if j == fort else TXT2, 9.5,
                '600' if j == fort else None)

c4, h4 = cartouche(X2, R4 + 150, 330, [
    u'⚠ « INSÉRER UN NŒUD ICI » est la seule',
    u'   entrée qu’on ne peut pas obtenir',
    u'   autrement : sinon couper, poser,',
    u'   rebrancher deux fois. Quatre gestes',
    u'   pour un.',
    u'✅ Un SEUL panneau de recherche, filtré',
    u'   ou non. Deux panneaux d’apparence',
    u'   différente feraient croire à deux',
    u'   catalogues de nœuds. C’est la règle du',
    u'   fil d’Ariane, appliquée une 2ᵉ fois.',
], u'les deux points qui ne s’inventent pas')
s += c4

# ---- 5 - LA GRILLE DU CANEVAS, EN SPECIMEN ---------------------------------
# Le fond des huit planches etait un <pattern> de points, et il est devenu PLAT :
# Lunacy importe les remplissages a motif vides, et 6 640 cercles par planche
# la rendraient inutilisable a l edition.
# Mais « la grille est faite de POINTS, pas de lignes » est une decision (§ 1,
# VUE sur la principale). On ne la perd donc pas : on la montre LA OU ELLE SE
# DECIDE, en points explicites, sur la planche du canevas -- et une seule fois.
GX, GY, GW, GH = 34, H - 210, 300, 130
s += lab(GX, GY - 34, '5', u'LA GRILLE DU CANEVAS — des POINTS, jamais des lignes',
         u'pas de 22 px · point de 1 px · #2b2b33 sur le fond #17171b')
s += '<rect x="%s" y="%s" width="%s" height="%s" fill="#17171b" stroke="#2E2E36"/>\n' % (
    GX, GY, GW, GH)
s += pointilles(GX, GY, GW, GH)
s += tt(GX + GW + 18, GY + 18, u'⚠ CE CARRÉ EST LE SEUL ENDROIT où la grille est dessinée.', ORANGE, 10.5, '600')
s += tt(GX + GW + 18, GY + 34, u'Le fond des huit planches est PLAT depuis le 23/08 : un', TXT2, 10)
s += tt(GX + GW + 18, GY + 48, u'remplissage à motif s’importe VIDE dans Lunacy, et Rodolf', TXT2, 10)
s += tt(GX + GW + 18, GY + 62, u'importe ces SVG directement. Le sens est gardé, le moyen a', TXT2, 10)
s += tt(GX + GW + 18, GY + 76, u'changé — même règle que la hachure et le damier (planches 03, 06).', TXT2, 10)
s += tt(GX + GW + 18, GY + 100, u'✅ Les lignes ont été VUES sur quatre références, et ÉCARTÉES :', u'#7FB77E', 10.5, '600')
s += tt(GX + GW + 18, GY + 114, u'la principale montre des POINTS, et c’est elle qui fait foi.', TXT2, 10)

s += tt(34, H - 30, u'⚠ PLANCHE D’ÉTUDE — les six éléments sont B6, B7, B9, E1, E2 et E5 d’ELEMENTS_A_DESSINER.md. '
        u'Les RATIOS de la spécification font foi, jamais les pixels de cette planche.', TXT3, 10.5)
ecrire('planche_07_canevas.svg', s)
rendre('planche_07_canevas', W, H)
