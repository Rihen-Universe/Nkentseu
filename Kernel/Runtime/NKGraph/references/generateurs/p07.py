# -*- coding: utf-8 -*-
# Planche 07 -- les six elements tranches le 23/08 apres la planche 06.
#
# Elle existe pour UNE raison : deux des six decisions sont des COLLISIONS, et
# une collision ne se demontre pas par une phrase. Il faut poser les deux objets
# cote a cote et regarder si on les confond. Le reste de la planche montre les
# quatre decisions ou la reponse a ete « ne rien ajouter » -- et pour celles-la
# l image sert a prouver que la marque existante SUFFIT.
import sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from gen import *

W, H = 1760, 1050
s = head(W, H, u'Planche 07 — le lasso, le menu, la recherche vide, la source, le puits, le commentaire',
         u'six éléments tranchés le 23/08 — dont une COLLISION, qui ne se démontre qu’en posant les deux objets côte à côte')


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


# ── 1 · LA COLLISION LASSO / CADRE ───────────────────────────────────────────
R1 = 104
s += lab(34, R1, '1', u'LE LASSO CONTRE LE CADRE — la collision qui a fait refuser le pointillé',
         u'les deux sont des RECTANGLES posés sur le fond, et tirer un rectangle est justement le geste qui CRÉE un cadre')

s += tt(34, R1 + 46, u'❌ CE QUI ÉTAIT ÉCRIT — les deux en pointillé', u'#E06C6C', 11.5, '600')
s += cadre(34, R1 + 62)
s += tt(34, R1 + 210, u'un CADRE — permanent', TXT2, 10.5)
s += ('<rect x="300" y="%d" width="230" height="130" rx="4" fill="none" stroke="%s" '
      'stroke-width="1.5" stroke-dasharray="4 4"/>\n') % (R1 + 62, ORANGE)
s += tt(300, R1 + 210, u'un LASSO — transitoire', TXT2, 10.5)
s += tt(300, R1 + 225, u'⚠ même endroit, même forme, même trait', u'#E06C6C', 10, '600')

s += tt(600, R1 + 46, u'✅ CE QUI EST DÉCIDÉ — le lasso est PLEIN, et rempli', u'#7FB77E', 11.5, '600')
s += cadre(600, R1 + 62)
s += tt(600, R1 + 210, u'le CADRE ne change pas', TXT2, 10.5)
s += ('<rect x="866" y="%d" width="230" height="130" rx="4" fill="%s" opacity="0.08"/>\n'
      '<rect x="866" y="%d" width="230" height="130" rx="4" fill="none" stroke="%s" stroke-width="1.5"/>\n'
      ) % (R1 + 62, ORANGE, R1 + 62, ORANGE)
s += tt(866, R1 + 210, u'le LASSO n’est plus confondable', u'#7FB77E', 10.5, '600')
s += tt(866, R1 + 225, u'le remplissage dit la ZONE PRISE, pas son bord', TXT3, 10)

c1, h1 = cartouche(34, R1 + 244, 1062, [
    u'⚠ LE POINTILLÉ EST DÉJÀ EMPLOYÉ SIX FOIS : dictionnaire (sous une PRISE) · fil en cours de tirage (un FIL) · nœud désactivé (le fil',
    u'   TRAVERSANT) · prise convertible (un HALO) · cadre (un RECTANGLE) · lasso (un RECTANGLE).',
    u'✅ Quatre des six ne se gênent pas — prise, fil, halo : le signal n’est pas le pointillé, c’est L’ENDROIT. C’est la règle des trois gris.',
    u'❌ Les deux derniers occupent le MÊME endroit. Et la collision n’est pas seulement visuelle : tirer un rectangle sur le fond EST le',
    u'   geste qui crée un cadre — un rectangle pointillé sous le curseur dit littéralement « je suis en train de fabriquer un cadre ».',
    u'📌 Et le lasso a droit à un signal SIMPLE pour la raison déjà admise sur le survol : il est transitoire et attaché au curseur.',
], u'pourquoi le pointillé a été refusé alors qu’il était déjà écrit dans la spécification')
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
R2 = R1 + 244 + h1 + 44
s += lab(34, R2, '3', u'LA SOURCE, LE PUITS, LE COMMENTAIRE — trois fois « ne RIEN ajouter »',
         u'chacun avait déjà sa marque ; la décision a été de ne pas en inventer une seconde par-dessus')

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
s += tt(530, R2 + 150, u'COMMENTAIRE — au repos', u'#EEF2F6', 11, '600')
s += tt(530, R2 + 165, u'ni corps, ni filet, ni fond : ce n’est pas un nœud —', TXT3, 10)
s += tt(530, R2 + 179, u'et il passe DEVANT le fil, qui sinon le barrerait', TXT3, 10)

s += '<rect x="800" y="%d" width="292" height="26" rx="3" fill="#1A1A1A" opacity="0.6"/>\n' % (R2 + 44)
s += tt(806, R2 + 62, u'ce bloc gère le cas où la texture manque', TXT3, 13)
s += tt(800, R2 + 150, u'COMMENTAIRE — au survol seulement', u'#EEF2F6', 11, '600')
s += tt(800, R2 + 165, u'⚠ signal FAIBLE volontairement : plus marqué, il prendrait', ORANGE, 10)
s += tt(800, R2 + 179, u'l’allure d’un nœud sans en-tête — ce qu’on vient d’interdire', TXT3, 10)

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

s += tt(34, H - 30, u'⚠ PLANCHE D’ÉTUDE — les six éléments sont B6, B7, B9, E1, E2 et E5 d’ELEMENTS_A_DESSINER.md. '
        u'Les RATIOS de la spécification font foi, jamais les pixels de cette planche.', TXT3, 10.5)
ecrire('planche_07_canevas.svg', s)
rendre('planche_07_canevas', W, H)
