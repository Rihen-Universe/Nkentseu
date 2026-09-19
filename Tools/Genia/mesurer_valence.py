# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
# Meme instrument de valence, applique a NOS maillages, pour le CONTRASTE.
import sys
import numpy as np
from collections import defaultdict, Counter
for chemin in sys.argv[1:]:
    V, F = [], []
    for ligne in open(chemin, encoding='utf-8', errors='replace'):
        if ligne.startswith('v '):
            V.append([float(x) for x in ligne.split()[1:4]])
        elif ligne.startswith('f '):
            F.append([int(t.split('/')[0]) - 1 for t in ligne.split()[1:]])
    V = np.array(V)
    cle, rep = {}, np.empty(len(V), dtype=np.int64)
    for i, p in enumerate(V):
        k = (round(float(p[0]),5), round(float(p[1]),5), round(float(p[2]),5))
        if k not in cle: cle[k] = len(cle)
        rep[i] = cle[k]
    t = Counter(len(f) for f in F)
    vois = defaultdict(set)
    for vs in F:
        n = len(vs)
        for j in range(n):
            a, b = rep[vs[j]], rep[vs[(j+1)%n]]
            vois[a].add(b); vois[b].add(a)
    val = Counter(len(v) for v in vois.values())
    tot = sum(val.values()) or 1
    q4 = val.get(4,0)*100.0/tot
    dom = val.most_common(1)[0][0]
    nom = chemin.split('/')[-1]
    print('%-32s V=%-7d F=%-7d  tris=%-7d quads=%-7d | valence dominante=%d  val4=%.1f %%'
          % (nom, len(cle), len(F), t.get(3,0), t.get(4,0), dom, q4))
