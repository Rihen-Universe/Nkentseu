# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# Compare deux PNG au pixel. Rend le nombre de pixels differents, et dit si les
# tailles different (auquel cas la comparaison ne vaut rien et c'est dit).
# Usage : python comparer_png.py a.png b.png [etiquette]
# Option : --temoin-positif a.png  -> modifie UN pixel d'une copie en memoire et exige
#          que le comparateur le voie (sinon il ne voit rien et ses zeros ne valent rien).
import sys
from PIL import Image
import numpy as np


def charger(p):
    return np.asarray(Image.open(p).convert('RGB')).astype(np.int32)


if len(sys.argv) >= 3 and sys.argv[1] == '--temoin-positif':
    a = charger(sys.argv[2])
    b = a.copy()
    b[0, 0, 0] = (b[0, 0, 0] + 128) % 256
    d = int((a != b).any(axis=2).sum())
    print('TEMOIN POSITIF (un pixel modifie en memoire) : %d pixel(s) vu(s) -- %s'
          % (d, 'le comparateur VOIT' if d == 1 else 'ECHEC : le comparateur ne voit pas'))
    sys.exit(0 if d == 1 else 1)

if len(sys.argv) < 3:
    print('usage : comparer_png.py a.png b.png [etiquette]')
    sys.exit(2)

a, b = charger(sys.argv[1]), charger(sys.argv[2])
etiq = sys.argv[3] if len(sys.argv) > 3 else ''
if a.shape != b.shape:
    print('%s : TAILLES DIFFERENTES %s contre %s -- comparaison SANS VALEUR' % (etiq, a.shape, b.shape))
    sys.exit(3)
masque = (a != b).any(axis=2)
n = int(masque.sum())
if n:
    ys, xs = np.where(masque)
    print('%s : %d pixel(s) different(s) sur %d ; zone x %d..%d y %d..%d'
          % (etiq, n, masque.size, xs.min(), xs.max(), ys.min(), ys.max()))
else:
    print('%s : 0 pixel different sur %d (taille %dx%d)' % (etiq, masque.size, a.shape[1], a.shape[0]))
sys.exit(0)
