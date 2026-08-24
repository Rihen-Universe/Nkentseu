# -*- coding: utf-8 -*-
"""C5 - planche d'essai : a partir de quel eclaircissement un fil selectionne
se distingue-t-il du meme fil non selectionne ?

On mesure DEUX choses, parce qu'aucune des deux ne suffit seule :
  - l'ecart PERCEPTUEL, en CIEDE2000, entre la couleur d'origine et l'eclaircie
    (la meme metrique que le § 4.2, qui a fixe un plancher de 11,0 pour
    distinguer DEUX TYPES cote a cote) ;
  - ce que ca donne A L'OEIL, sur le fond reel du canevas, en PNG regarde.
"""
import io, math, os, sys

SORTIE = os.path.dirname(os.path.abspath(__file__))

FOND = "#17171b"
FAMILLES = [("execution", "#F79A28"), ("nombre", "#17B2EB"), ("geometrie", "#C0EB81"),
            ("texte", "#F2559B"), ("apparence", "#D9B6A3"), ("reference", "#81EBEB"),
            ("quelconque", "#9AA3AD")]
TAUX = [0.0, 0.20, 0.35, 0.50]

# ---------------------------------------------------------------- couleurs ---
def hex2rgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i+2], 16) for i in (0, 2, 4))

def rgb2hex(c):
    return '#%02X%02X%02X' % tuple(max(0, min(255, int(round(v)))) for v in c)

def eclaircir(h, k):
    """Melange vers le blanc : c' = c + (255 - c) * k."""
    return rgb2hex(tuple(v + (255 - v) * k for v in hex2rgb(h)))

def srgb_lin(v):
    v /= 255.0
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4

def rgb2lab(h):
    r, g, b = (srgb_lin(v) for v in hex2rgb(h))
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 1.00000
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
    f = lambda t: t ** (1.0/3) if t > 216.0/24389 else (841.0/108) * t + 4.0/29
    fx, fy, fz = f(x), f(y), f(z)
    return (116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz))

def ciede2000(h1, h2):
    L1, a1, b1 = rgb2lab(h1); L2, a2, b2 = rgb2lab(h2)
    C1 = math.hypot(a1, b1); C2 = math.hypot(a2, b2)
    Cb = (C1 + C2) / 2.0
    G = 0.5 * (1 - math.sqrt(Cb**7 / (Cb**7 + 25.0**7))) if Cb > 0 else 0.5
    a1p, a2p = (1 + G) * a1, (1 + G) * a2
    C1p, C2p = math.hypot(a1p, b1), math.hypot(a2p, b2)
    h1p = math.degrees(math.atan2(b1, a1p)) % 360 if (a1p or b1) else 0.0
    h2p = math.degrees(math.atan2(b2, a2p)) % 360 if (a2p or b2) else 0.0
    dLp = L2 - L1; dCp = C2p - C1p
    if C1p * C2p == 0:
        dhp = 0.0
    else:
        d = h2p - h1p
        dhp = d - 360 if d > 180 else (d + 360 if d < -180 else d)
    dHp = 2 * math.sqrt(C1p * C2p) * math.sin(math.radians(dhp) / 2)
    Lbp = (L1 + L2) / 2.0; Cbp = (C1p + C2p) / 2.0
    if C1p * C2p == 0:
        hbp = h1p + h2p
    else:
        s = h1p + h2p
        hbp = (s + 360) / 2 if abs(h1p - h2p) > 180 and s < 360 else \
              ((s - 360) / 2 if abs(h1p - h2p) > 180 else s / 2)
    T = (1 - 0.17 * math.cos(math.radians(hbp - 30)) + 0.24 * math.cos(math.radians(2 * hbp))
         + 0.32 * math.cos(math.radians(3 * hbp + 6)) - 0.20 * math.cos(math.radians(4 * hbp - 63)))
    dTh = 30 * math.exp(-(((hbp - 275) / 25.0) ** 2))
    Rc = 2 * math.sqrt(Cbp**7 / (Cbp**7 + 25.0**7)) if Cbp > 0 else 0.0
    Sl = 1 + (0.015 * (Lbp - 50) ** 2) / math.sqrt(20 + (Lbp - 50) ** 2)
    Sc = 1 + 0.045 * Cbp
    Sh = 1 + 0.015 * Cbp * T
    Rt = -math.sin(math.radians(2 * dTh)) * Rc
    return math.sqrt((dLp/Sl)**2 + (dCp/Sc)**2 + (dHp/Sh)**2 + Rt * (dCp/Sc) * (dHp/Sh))

# ------------------------------------------------------------------ mesure ---
print("ECART PERCEPTUEL (CIEDE2000) entre le fil normal et le fil eclairci")
print("  rappel : le plancher du § 4.2 est 11,0 -- mais il vaut pour distinguer")
print("  DEUX TYPES cote a cote, ce qui n'est pas le meme travail qu'ici.\n")
print("  %-12s %8s %8s %8s" % ("famille", "+20 %", "+35 %", "+50 %"))
mesures = {}
for nom, coul in FAMILLES:
    d = [ciede2000(coul, eclaircir(coul, k)) for k in TAUX[1:]]
    mesures[nom] = d
    print("  %-12s %8.2f %8.2f %8.2f" % (nom, d[0], d[1], d[2]))
mini = [min(mesures[n][i] for n, _ in FAMILLES) for i in range(3)]
print("\n  %-12s %8.2f %8.2f %8.2f   <-- le maillon faible commande" % ("MINIMUM", *mini))
pire = [min(((mesures[n][i], n) for n, _ in FAMILLES))[1] for i in range(3)]
print("  %-12s %8s %8s %8s" % ("(famille)", *pire))

# ------------------------------------------------------------------ planche ---
LARG, LIGNE = 1180, 92
HAUT = 150 + len(FAMILLES) * LIGNE + 90
s = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
     'font-family="Segoe UI, Inter, sans-serif">' % (LARG, HAUT, LARG, HAUT)]
s.append('<rect width="%d" height="%d" fill="%s"/>' % (LARG, HAUT, FOND))
s.append('<text x="30" y="42" fill="#EEF2F6" font-size="19" font-weight="600">'
         'C5 — le fil sélectionné : à partir de quel éclaircissement ?</text>')
s.append('<text x="30" y="66" fill="#8A8A8A" font-size="12">'
         'même fil, même épaisseur — seule la luminosité change. Écart CIEDE2000 sous chaque colonne.</text>')
for i, k in enumerate(TAUX):
    x = 250 + i * 220
    lib = "normal" if k == 0 else "+%d %%" % int(k * 100)
    s.append('<text x="%d" y="112" fill="#C8CCD4" font-size="13" font-weight="600" '
             'text-anchor="middle">%s</text>' % (x + 70, lib))
    if i:
        s.append('<text x="%d" y="130" fill="#6A6A6A" font-size="10.5" text-anchor="middle">'
                 'ΔE min %.1f</text>' % (x + 70, mini[i-1]))
for j, (nom, coul) in enumerate(FAMILLES):
    y = 175 + j * LIGNE
    s.append('<text x="30" y="%d" fill="#C8CCD4" font-size="12.5">%s</text>' % (y + 4, nom))
    s.append('<text x="30" y="%d" fill="#6A6A6A" font-size="10">%s</text>' % (y + 20, coul))
    for i, k in enumerate(TAUX):
        x = 250 + i * 220
        c = coul if k == 0 else eclaircir(coul, k)
        s.append('<path d="M%d %d C%d %d, %d %d, %d %d" stroke="%s" stroke-width="2" fill="none"/>'
                 % (x, y - 14, x + 46, y - 14, x + 94, y + 14, x + 140, y + 14, c))
        # la meme chose AVEC les deux poignees, 26 px plus bas
        yb = y + 34
        s.append('<path d="M%d %d C%d %d, %d %d, %d %d" stroke="%s" stroke-width="2" fill="none"/>'
                 % (x, yb - 8, x + 46, yb - 8, x + 94, yb + 8, x + 140, yb + 8, c))
        if k:
            for px, py in ((x, yb - 8), (x + 140, yb + 8)):
                s.append('<rect x="%d" y="%d" width="7" height="7" rx="1" fill="%s"/>'
                         % (px - 3.5, py - 3.5, c))
s.append('<text x="30" y="%d" fill="#6A6A6A" font-size="10.5">'
         'ligne du haut : sans poignées · ligne du bas : avec les deux poignées de 7 px. '
         'Fond réel du canevas #17171b.</text>' % (HAUT - 46))
s.append('<text x="30" y="%d" fill="#6A6A6A" font-size="10.5">'
         '⚠ PLANCHE D’ESSAI — elle sert à MESURER un seuil, pas à figer un dessin.</text>' % (HAUT - 26))
s.append('</svg>')
chemin = os.path.join(SORTIE, "essai_c5.svg")
open(chemin, 'wb').write(("\n".join(s) + "\n").encode('utf-8'))
print("\necrit %s  (%d x %d)" % (chemin, LARG, HAUT))
