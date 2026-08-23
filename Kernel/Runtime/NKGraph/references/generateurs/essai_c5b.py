# -*- coding: utf-8 -*-
"""C5 (suite) — le renfort a-t-il un cas d'usage, ou est-il un decor ?

LA QUESTION : quand un fil selectionne est si long que SES DEUX POIGNEES sont
hors du cadre, il ne reste que l'eclaircissement. A +35 %, l'ecart percu vaut
5,23 -- sous le plancher de 11,0. Le renfort sert-il quand meme ?

⚠️ CE QUI CHANGE ET QUI N'ETAIT PAS DANS LA PREMIERE MESURE : le plancher de
11,0 et les 5,23 ont ete mesures pour une comparaison TEMPORELLE (le meme fil,
avant et apres, de memoire). Ici la comparaison est SPATIALE et SIMULTANEE :
plusieurs fils de la meme couleur sont visibles en meme temps, un seul est
eclairci. Le seuil n'est pas le meme, et il ne se deduit pas -- il se regarde.
"""
import io, os

SORTIE = os.path.dirname(os.path.abspath(__file__))
FOND = "#17171b"
BLEU = "#17B2EB"


def eclaircir(h, k):
    h = h.lstrip('#')
    c = [int(h[i:i + 2], 16) for i in (0, 2, 4)]
    return '#%02X%02X%02X' % tuple(int(round(v + (255 - v) * k)) for v in c)


def faisceau(x, y, w, n, indice, k, poignees):
    """n fils paralleles ; celui d'indice `indice` est eclairci de k.
    Si poignees est faux, les fils SORTENT du cadre des deux cotes."""
    out = []
    for i in range(n):
        yy = y + i * 30
        c = eclaircir(BLEU, k) if (i == indice and k) else BLEU
        x1, x2 = (x, x + w) if poignees is True else (x - 40, x + w + 40)
        out.append('<path d="M%s %sC%s %s %s %s %s %s" fill="none" stroke="%s" stroke-width="2"/>'
                   % (x1, yy, x1 + 90, yy, x2 - 90, yy + 16, x2, yy + 16, c))
        if poignees == 'bord' and i == indice:
            # le fil coupe le bord du cadre : on y pose un chevron vers l exterieur
            for bx, sens, by in ((x, -1, yy + 3), (x + w, 1, yy + 13)):
                out.append('<path d="M%s %s l%s -5 l0 10 z" fill="%s"/>'
                           % (bx + sens * 9, by, -sens * 9, BLEU))
            continue
        if poignees is True and i == indice and k:
            for px, py in ((x1, yy), (x2, yy + 16)):
                out.append('<rect x="%s" y="%s" width="7" height="7" rx="1" fill="%s"/>'
                           % (px - 3.5, py - 3.5, c))
    return "\n".join(out)


# cas f : la poignee ne DISPARAIT pas quand la prise sort du cadre -- elle se
# pose la ou le fil COUPE le bord, en chevron pointant vers l exterieur. Le
# signal reste a l ecran, et il informe encore : il dit de quel cote est la prise.
BORD = True
CAS = [(u"a · aucun fil sélectionné", None, 0.0, True),
       (u"b · le 3ᵉ sélectionné, POIGNÉES VISIBLES", 2, 0.35, True),
       (u"c · le 3ᵉ sélectionné, poignées HORS CADRE — +35 %", 2, 0.35, False),
       (u"d · le 3ᵉ sélectionné, poignées HORS CADRE — +50 %", 2, 0.50, False),
       (u"e · le 3ᵉ sélectionné, poignées HORS CADRE — +65 %", 2, 0.65, False),
       (u"f · poignées hors cadre, mais CHEVRON SUR LE BORD — sans aucun éclaircissement", 2, 0.0, 'bord')]

W, LIG = 1500, 210
H = 130 + len(CAS) * LIG + 70
s = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
     'font-family="Segoe UI, Inter, sans-serif">' % (W, H, W, H)]
s.append('<rect width="%d" height="%d" fill="%s"/>' % (W, H, FOND))
s.append('<text x="30" y="42" fill="#EEF2F6" font-size="19" font-weight="600">'
         'C5 — le renfort sert-il quand les poignées sont hors cadre ?</text>')
s.append('<text x="30" y="66" fill="#8A8A8A" font-size="12">'
         'cinq fils de la MÊME couleur. Un seul est éclairci. Sauras-tu dire lequel, '
         'sans les poignées ?</text>')
s.append('<text x="30" y="86" fill="#6A6A6A" font-size="11">'
         'la comparaison est ici SPATIALE et simultanée — pas temporelle comme dans la '
         'première mesure. Le seuil n’est pas le même.</text>')

for j, (nom, idx, k, poi) in enumerate(CAS):
    y = 135 + j * LIG
    s.append('<text x="30" y="%d" fill="#F79A28" font-size="12.5" font-weight="600">%s</text>'
             % (y, nom))
    s.append('<rect x="300" y="%d" width="1000" height="%d" fill="none" stroke="#2B2B33" '
             'stroke-dasharray="3 3"/>' % (y - 18, LIG - 46))
    s.append('<text x="1310" y="%d" fill="#6A6A6A" font-size="10">le cadre visible</text>'
             % (y - 6))
    s.append(faisceau(300, y + 16, 1000, 5, idx if idx is not None else -1, k, poi))
    if idx is not None and not poi:
        s.append('<text x="30" y="%d" fill="#6A6A6A" font-size="10.5">réponse : le 3ᵉ</text>'
                 % (y + 20))

s.append('<text x="30" y="%d" fill="#6A6A6A" font-size="10.5">'
         '⚠ PLANCHE D’ESSAI — elle sert à trancher si un renfort imperceptible isolément '
         'devient utile en comparaison simultanée.</text>' % (H - 30))
s.append('</svg>')
chemin = os.path.join(SORTIE, "essai_c5b.svg")
open(chemin, 'wb').write(("\n".join(s) + "\n").encode('utf-8'))
print("ecrit %s (%d x %d)" % (chemin, W, H))
