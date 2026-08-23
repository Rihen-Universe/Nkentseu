# -*- coding: utf-8 -*-
u"""Controle des PLANCHES -- le troisieme document, que rien ne surveillait.

POURQUOI IL EXISTE
==================
`verifie_coherence.py` compare ELEMENTS_A_DESSINER.md et
SPECIFICATION_VISUELLE.md. Il fait bien son travail. Mais le 23/08 Rodolf
a signale que la planche 01 affirmait « toujours pas tranche » a propos
de cinq points TRANCHES depuis, et le controle n'avait rien dit.

⚠️ Ce n'est pas un trou dans le controle : c'est un trou dans son
PERIMETRE. Il regarde deux documents, et il y a TROIS documents qui
affirment des choses -- l'inventaire, la specification, et LES PLANCHES.
Les planches sont celles que Rodolf regarde ; c'est donc celles-la qui
lui mentent, et c'etait le seul des trois que personne ne relisait.

> Un controle en aval du bon endroit valide la forme de l'echec, pas son
> fond. Ici il n'etait meme pas en aval : il etait a cote.

CE QU'IL VERIFIE -- DEUX CHOSES, ET ELLES N'ONT RIEN A VOIR
===========================================================

  A. LE TEXTE SE MARCHE-T-IL DESSUS ?
     Mesure REELLE, avec la fonte de rendu (Segoe UI, celle que le SVG
     demande et que le navigateur utilise), et pas une estimation en
     « nombre de caracteres x largeur moyenne ».
     Il attrape deux familles :
       - texte contre texte : le titre du panneau 6 passait SOUS celui du
         panneau 7 parce qu'il faisait 63 caracteres dans une colonne de
         346 px ;
       - texte contre RECTANGLE OPAQUE POSTERIEUR : la legende du
         panneau 7 etait mangee par le noeud du panneau 9, dessine apres
         elle. En SVG, « apres » veut dire « par-dessus ».

     ⚠️ Ce controle a ete ECRIT parce qu'on avait conclu qu'il etait
     impossible (« SVG n'a aucune notion de debordement »). C'est vrai de
     SVG ; ce n'est pas vrai de nous. La fonte est sur le disque, les
     coordonnees sont dans le fichier, la mesure se fait.

  B. LA PLANCHE DIT-ELLE « PAS TRANCHE » DE CE QUI EST TRANCHE ?
     Un generateur peut declarer, dans une constante OUVERTS_DECLARES,
     les points qu'il presente encore comme ouverts, chacun avec sa
     source. Le controle va lire l'etat de cette source.
     C'est ce qui manquait : une affirmation qui n'est pas ecrite dans
     une forme lisible par une machine ne peut etre verifiee par
     personne, et celle-la etait tenue a la main depuis le 21/08.

CE QU'IL NE VERIFIE PAS, ET IL FAUT LE DIRE
===========================================
  - Un dessin FAUX aux bonnes dimensions. Les prises carrees de la v3
    tenaient parfaitement dans leur rangee : aucun controle geometrique
    ne pouvait les voir. Seul le coup d'oeil, ou une mesure du RATIO
    contre la specification, les attrape. Ici c'est le coup d'oeil.
  - Le texte pose sur un rectangle ANTERIEUR (un cartouche de fond, par
    exemple) : c'est le cas NORMAL, il n'est pas signale.
  - Les elements dans un `transform` : aucune planche n'en a. Si une en
    prend un, ce controle deviendra faux SANS LE DIRE -- il refuse donc
    de tourner sur un fichier qui contient `transform=`.

USAGE
=====
  cd Kernel/Runtime/NKGraph && python ../../../verifie_planches.py
  ou depuis la racine :  python verifie_planches.py
Code de retour 1 si une collision ou une divergence est detectee.
"""
import io
import os
import re
import sys

# La console Windows est en cp1252 : afficher un emoji y leve une
# exception et TUE le controle au milieu de son rapport. Un controle
# qui meurt de son propre message d'alerte ne signale rien -- il a
# meme l'air d'avoir fini. Mesure : il mourait sur le message qui dit
# qu'aucun generateur ne declare ses points ouverts.
try:
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception:
    pass

RACINE = os.path.dirname(os.path.abspath(__file__))
NKG = os.path.join(RACINE, 'Kernel', 'Runtime', 'NKGraph')
REFS = os.path.join(NKG, 'references')
GENS = os.path.join(REFS, 'generateurs')
SPEC = os.path.join(NKG, 'SPECIFICATION_VISUELLE.md')
INV = os.path.join(NKG, 'ELEMENTS_A_DESSINER.md')

# La fonte que le SVG demande en premier. Si elle manque, on ARRETE :
# mesurer avec une autre fonte donnerait des largeurs plausibles et
# fausses, et un controle plausible et faux est pire que pas de controle.
FONTE = 'C:/Windows/Fonts/segoeui.ttf'
# ⚠️ font-weight 600 est un SEMI-GRAS, et Segoe UI Semibold n'est pas
# installe ici (seuls segoeui.ttf et segoeuib.ttf existent). On mesure
# donc le semi-gras avec le GRAS, qui est plus large : le controle
# SUR-estime ces largeurs de quelques pourcents. C'est le bon sens de
# l'erreur -- il signalera un cas serre plutot que d'en rater un -- mais
# c'est une approximation, et elle est ecrite plutot que tue.
FONTE_GRAS = 'C:/Windows/Fonts/segoeuib.ttf'

RE_BALISE = re.compile(r'<(/?)([a-zA-Z][\w-]*)([^>]*?)(/?)>', re.S)
RE_ATTR = re.compile(r'([\w:-]+)\s*=\s*"([^"]*)"')

# ⚠️ patternTransform et gradientTransform ne DEPLACENT RIEN : ils
# orientent un motif a l'interieur de sa propre case. Les confondre avec
# transform a fait refuser la planche 02 en bloc, alors qu'elle n'a
# aucune coordonnee transformee -- un controle qui refuse a tort est un
# controle qu'on desactive. On ne cherche donc que transform= precede
# d'un blanc, et jamais dans <defs>.
RE_TRANSFORM = re.compile(r'\stransform\s*=')
# Les seuls transform que ces planches emploient sont des translations
# pures : planche 02 decale de +/- 1,7 px les deux traits d'un fil
# double. Refuser TOUTE la planche pour ca serait le controle qui
# refuse a tort -- on les LIT donc, et on ne refuse que ce qu'on ne
# sait pas lire (rotation, echelle, matrice), qui deplacerait vraiment
# les coordonnees sans qu'on s'en apercoive.
RE_TSPART = re.compile(r'(translate|scale)\(([^)]*)\)')
RE_NOMBRE = re.compile(r'-?\d+(?:\.\d+)?')


def transforme(t):
    u"""Rend (dx, dy, echelle) pour une suite de translate/scale.

    Rien d'autre n'est accepte. Une rotation ou une matrice
    deplacerait les coordonnees d'une facon que ce controle ne
    saurait pas suivre, et il rendrait un vert pour un fichier
    qu'il n'a pas su lire -- c'est exactement le PNG aux bonnes
    dimensions qui etait la page d'erreur du navigateur.
    """
    dx = dy = 0.0
    ech = 1.0
    reste = t
    for m in RE_TSPART.finditer(t):
        reste = reste.replace(m.group(0), '', 1)
        v = [float(x) for x in RE_NOMBRE.findall(m.group(2))]
        if m.group(1) == 'translate':
            if not v:
                return None
            dx += ech * v[0]
            dy += ech * (v[1] if len(v) > 1 else 0.0)
        else:
            if not v or (len(v) > 1 and v[0] != v[1]):
                return None      # echelle non uniforme : non lu
            ech *= v[0]
    if reste.strip():
        return None
    return (dx, dy, ech)


HERITES = ('font-size', 'font-weight', 'text-anchor', 'opacity', 'fill')


def attrs(s):
    return dict(RE_ATTR.findall(s))


def nombre(d, cle, defaut=None):
    v = d.get(cle)
    if v is None:
        return defaut
    try:
        return float(v)
    except ValueError:
        return defaut


class Mesureur(object):
    u"""Largeur d'une chaine, avec la vraie fonte et le vrai corps."""

    def __init__(self):
        from PIL import ImageFont
        self._IF = ImageFont
        self._cache = {}
        for f in (FONTE, FONTE_GRAS):
            if not os.path.isfile(f):
                raise SystemExit(
                    'REFUS : la fonte %s est absente. Mesurer avec une autre\n'
                    '  fonte rendrait des largeurs plausibles et fausses.' % f)

    def _fonte(self, taille, gras):
        cle = (int(round(taille)), gras)
        if cle not in self._cache:
            self._cache[cle] = self._IF.truetype(
                FONTE_GRAS if gras else FONTE, cle[0])
        return self._cache[cle]

    def largeur(self, texte, taille, gras):
        return self._fonte(taille, gras).getlength(texte)


def demarque(s):
    u"""Le contenu d'un <text> : on retire les balises et les entites."""
    s = re.sub(r'<[^>]*>', '', s)
    return (s.replace('&amp;', '&').replace('&lt;', '<')
             .replace('&gt;', '>').replace('&#160;', ' '))


def boites(chemin, mes):
    u"""Rend (textes, rects), chacun avec son INDICE dans le document.

    L'indice compte : en SVG, ce qui est ecrit plus loin est peint
    par-dessus. C'est toute la difference entre « une legende sur un
    cartouche » (normal) et « une legende sous un noeud » (le defaut).

    ⚠️ LES ATTRIBUTS S'HERITENT DU <g> PARENT, et c'est ce que la
    premiere version de ce controle ignorait. Elle a signale trois
    pastilles de la planche 01 comme cachees : leur <g> portait
    font-size="8.5" et text-anchor="middle", donc le controle mesurait
    des glyphes de 3 lettres a 12 px et alignes a gauche -- des boites
    une fois et demie trop larges, decalees d'une demi-largeur.
    Trois faux positifs sur quatre signalements. Un controle qui crie au
    loup finit ignore, puis desactive : c'est ecrit en tete de
    verifie_coherence.py, et je venais de l'ecrire moi-meme.
    """
    src = io.open(chemin, encoding='utf-8').read()

    textes, rects = [], []
    pile = [{}]            # attributs herites
    decal = [(0.0, 0.0, 1.0)]   # translation et echelle cumulees
    profondeur_defs = 0
    i = 0
    while True:
        m = RE_BALISE.search(src, i)
        if not m:
            break
        i = m.end()
        fermante, nom, brut, autoferme = m.group(1), m.group(2), m.group(3), m.group(4)

        if nom == 'defs':
            if fermante:
                profondeur_defs -= 1
            elif not autoferme:
                profondeur_defs += 1
            continue
        if profondeur_defs > 0:
            continue

        if fermante:
            if nom == 'g' and len(pile) > 1:
                pile.pop()
                decal.pop()
            continue

        a = attrs(brut)
        dx, dy, ech = decal[-1]
        if 'transform' in a:
            t = transforme(a['transform'])
            if t is None:
                raise SystemExit(
                    'REFUS : %s porte transform="%s", que ce controle ne sait\n'
                    '  pas lire. Il lit les coordonnees comme absolues ; il\n'
                    '  rendrait un vert pour un fichier qu il n a pas su lire.'
                    % (os.path.basename(chemin), a['transform']))
            dx += ech * t[0]
            dy += ech * t[1]
            ech *= t[2]

        if nom == 'g':
            h = dict(pile[-1])
            for k in HERITES:
                if k in a:
                    h[k] = a[k]
            if not autoferme:
                pile.append(h)
                decal.append((dx, dy, ech))
            continue

        # attributs effectifs : les siens, sinon ceux du <g> parent
        eff = dict(pile[-1])
        eff.update(a)

        if nom == 'text':
            f = src.find('</text>', i)
            contenu = demarque(src[i:f]) if f >= 0 else ''
            if f >= 0:
                i = f + 7
            if not contenu.strip():
                continue
            x, y = nombre(eff, 'x'), nombre(eff, 'y')
            if x is None or y is None:
                continue
            x, y = dx + ech * x, dy + ech * y
            taille = nombre(eff, 'font-size', 12.0) * ech
            gras = eff.get('font-weight') in ('600', '700', 'bold')
            w = mes.largeur(contenu, taille, gras)
            anc = eff.get('text-anchor', 'start')
            x0 = x - w if anc == 'end' else (x - w / 2.0 if anc == 'middle' else x)
            # y est la LIGNE DE BASE. Capitale ~0,72 du corps, jambage
            # ~0,21 : on prend un peu large, un controle qui rate est
            # pire qu'un controle qui signale un cas serre.
            textes.append((m.start(), x0, y - taille * 0.75, x0 + w,
                           y + taille * 0.22, contenu, taille))
        elif nom == 'rect':
            x = dx + ech * nombre(eff, 'x', 0.0)
            y = dy + ech * nombre(eff, 'y', 0.0)
            w = ech * nombre(eff, 'width', 0.0)
            h = ech * nombre(eff, 'height', 0.0)
            remp = eff.get('fill', '#000')
            op = nombre(eff, 'opacity', 1.0)
            # Un rectangle transparent, sans remplissage, en motif ou
            # tres pale ne cache rien : il ne peut pas etre le coupable.
            if (remp == 'none' or remp.startswith('url(') or op < 0.9
                    or w <= 0 or h <= 0):
                continue
            rects.append((m.start(), x, y, x + w, y + h, remp))
    return textes, rects


def croise(a, b, marge=0.0):
    return (a[1] < b[3] - marge and b[1] < a[3] - marge
            and a[2] < b[4] - marge and b[2] < a[4] - marge)


def controle_a(planches, mes):
    u"""Collision et debordement. Rend le nombre de defauts."""
    print('\n=== A. le texte se marche-t-il dessus ? ===')
    dur = 0
    for chemin in planches:
        nom = os.path.basename(chemin)
        textes, rects = boites(chemin, mes)
        # ⚠️ UN VERT SUR ZERO CAS MESURE EST UN MENSONGE. Ce controle l'a
        # rendu le 23/08, sur les SEPT planches a la fois : son parcours
        # de balises ne matchait rien (un caractere d'echappement abime),
        # et il a affiche « ok » sept fois. Il avait l'air d'avoir
        # travaille. Toute planche porte au moins ses titres de panneau ;
        # zero texte ne veut pas dire « propre », ca veut dire « pas lu ».
        if not textes:
            raise SystemExit(
                'REFUS : aucun texte lu dans %s. Ce controle ne mesure rien\n'
                '  et rendrait un vert pour un fichier qu il n a pas su lire.'
                % nom)
        src = io.open(chemin, encoding='utf-8').read(400)
        vb = re.search(r'viewBox="0 0 (\d+) (\d+)"', src)
        larg = float(vb.group(1)) if vb else None
        defauts = []

        for i in range(len(textes)):
            t = textes[i]
            if larg is not None and t[3] > larg - 4:
                defauts.append(u'DEBORDE la planche : « %s… » finit a %.0f (bord %.0f)'
                               % (t[5][:44], t[3], larg))
            for j in range(i + 1, len(textes)):
                u_ = textes[j]
                if croise((None,) + t[1:5], (None,) + u_[1:5], marge=1.0):
                    defauts.append(
                        u'TEXTE SUR TEXTE : « %s… » et « %s… »'
                        % (t[5][:38], u_[5][:38]))
            for r in rects:
                # seul un rectangle POSTERIEUR peint par-dessus
                if r[0] < t[0]:
                    continue
                if croise((None,) + t[1:5], (None,) + r[1:5], marge=1.0):
                    defauts.append(
                        u'TEXTE SOUS UN RECTANGLE POSTERIEUR %s : « %s… »'
                        % (r[5], t[5][:44]))
        if defauts:
            dur += len(defauts)
            print('  %s : %d' % (nom, len(defauts)))
            for d in sorted(set(defauts)):
                print('     - %s' % d)
        else:
            print('  %s : ok (%d textes, %d rectangles opaques)'
                  % (nom, len(textes), len(rects)))
    return dur


# ----------------------------------------------------------------------
# B. LES POINTS QU UNE PLANCHE PRESENTE ENCORE COMME OUVERTS
# ----------------------------------------------------------------------
# Source « recap » : la ligne N du tableau du § 13. Une ligne dont le
# numero est BARRE (~~N~~) est fermee -- la planche ne doit plus la
# presenter comme ouverte. Une ligne barree qui porte encore un 🔴 est
# MIXTE : le controle le dit au lieu de trancher a la place de l'humain.
#
# Source « inv » : une cle d'inventaire (D7, E3...). Fermee si sa colonne
# d'etat ne porte ni ⚠️ ni 🟡.
RE_RECAP = re.compile(u'^\\| (~~)?(\\d+)(~~)? \\|(.*)\\|$')


def etats_recap():
    etats = {}
    dedans = False
    for ligne in io.open(SPEC, encoding='utf-8').read().split('\n'):
        if ligne.startswith('## 13.'):
            dedans = True
            continue
        if dedans and ligne.startswith('## '):
            break
        if not dedans:
            continue
        m = RE_RECAP.match(ligne)
        if not m:
            continue
        barre = bool(m.group(1))
        corps = m.group(4)
        etats[m.group(2)] = ('FERME_MIXTE' if (barre and u'🔴' in corps)
                             else 'FERME' if barre else 'OUVERT')
    return etats


RE_INV = re.compile(u'^\\| ([A-E]\\d+[a-z]?) \\|.*\\| ([^|]*) \\|$')


def etats_inventaire():
    etats = {}
    for ligne in io.open(INV, encoding='utf-8').read().split('\n'):
        m = RE_INV.match(ligne)
        if m:
            et = m.group(2)
            etats[m.group(1)] = ('OUVERT' if (u'⚠️' in et or u'\U0001F7E1' in et)
                                 else 'FERME')
    return etats


def controle_b():
    print('\n=== B. une planche dit-elle « pas tranche » de ce qui est tranche ? ===')
    recap = etats_recap()
    inv = etats_inventaire()
    print('  § 13 : %d lignes lues · inventaire : %d lignes lues'
          % (len(recap), len(inv)))
    dur, declarants = 0, 0
    for f in sorted(os.listdir(GENS)):
        if not f.startswith('p') or not f.endswith('.py'):
            continue
        src = io.open(os.path.join(GENS, f), encoding='utf-8').read()
        m = re.search(r'^OUVERTS_DECLARES\s*=\s*\[(.*?)^\]',
                      src, re.S | re.M)
        if not m:
            continue
        declarants += 1
        lignes = re.findall(r"\(\s*'(\w+)'\s*,\s*'([\w.]+)'\s*,", m.group(1))
        print('  %s declare %d point(s) ouvert(s)' % (f, len(lignes)))
        hors = []
        for (source, cle) in lignes:
            if source == 'hors':
                # Un point dont l'etat ne vit dans aucune table lisible.
                # On ne le tait pas et on ne le compte pas : on le
                # DECLARE. Un controle qui range en silence ce qu'il ne
                # sait pas verifier finit par tout ranger.
                hors.append(cle)
                continue
            table = recap if source == 'recap' else inv
            etat = table.get(cle)
            if etat is None:
                dur += 1
                print(u'     🔴 RENVOI MORT : %s %s introuvable' % (source, cle))
            elif etat == 'FERME':
                dur += 1
                print(u'     🔴 DEJA TRANCHE : %s %s est ferme -- la planche '
                      u'ment a Rodolf' % (source, cle))
            elif etat == 'FERME_MIXTE':
                print(u'     🟡 NON CONCLUANT : %s %s est barre mais porte '
                      u'encore un point rouge. A verifier a la main.'
                      % (source, cle))
            else:
                print(u'     ✅ %s %s : encore ouvert' % (source, cle))
        if hors:
            print(u'     [hors controle] %d point(s) sans table lisible : %s'
                  % (len(hors), ', '.join(hors)))
    if not declarants:
        print(u'  ⚠️ AUCUN generateur ne declare OUVERTS_DECLARES.')
        print(u'     Ce controle ne peut donc rien dire, et il le DIT plutot')
        print(u'     que de rendre un vert. Un vert sur zero cas mesure est')
        print(u'     un mensonge -- c est « une moyenne sur un seul')
        print(u'     echantillon », en pire : sur aucun.')
    return dur


def main():
    mes = Mesureur()
    planches = sorted(
        os.path.join(REFS, f) for f in os.listdir(REFS)
        if f.startswith('planche_') and f.endswith('.svg'))
    if not planches:
        raise SystemExit('REFUS : aucune planche trouvee dans ' + REFS)
    dur = controle_a(planches, mes) + controle_b()
    print('')
    if dur:
        print('%d defaut(s). Code de retour 1.' % dur)
    else:
        print('OK : aucune collision, aucune divergence detectable.')
    return 1 if dur else 0


if __name__ == '__main__':
    sys.exit(main())
