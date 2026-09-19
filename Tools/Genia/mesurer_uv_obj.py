# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
# MEME instrument que pour l'etalon Meshy, applique a un .obj portant des UV.
# ATTENDU : si l'hypothese "la topologie reguliere EST la condition du depliage"
# est juste, alors NOTRE personnage_quads (99,5 % de valence 4) doit obtenir des
# chiffres du MEME ORDRE que Meshy : coutures sous 10 %, distorsion p99 sous 1,5.
import sys
import numpy as np
from collections import defaultdict

chemin = sys.argv[1]
V, VT, F, FT = [], [], [], []
for ligne in open(chemin, encoding='utf-8', errors='replace'):
    p = ligne.split()
    if not p: continue
    if p[0] == 'v':  V.append([float(x) for x in p[1:4]])
    elif p[0] == 'vt': VT.append([float(x) for x in p[1:3]])
    elif p[0] == 'f':
        vs, ts = [], []
        for t in p[1:]:
            m = t.split('/')
            vs.append(int(m[0]) - 1)
            ts.append(int(m[1]) - 1 if len(m) > 1 and m[1] else -1)
        F.append(vs); FT.append(ts)
V = np.array(V); uv = np.array(VT) if VT else np.zeros((0,2))
print(chemin.split('/')[-1], ': V=%d  VT=%d  F=%d' % (len(V), len(uv), len(F)))
if len(uv) == 0:
    print('   -> AUCUNE UV dans ce fichier. Rien a mesurer.'); sys.exit(0)
if any(t < 0 for ts in FT for t in ts):
    print('   -> des faces SANS indice UV. Rien a mesurer.'); sys.exit(0)

parent = list(range(len(uv)))
def find(a):
    while parent[a] != a: parent[a] = parent[parent[a]]; a = parent[a]
    return a
def union(a,b):
    ra, rb = find(a), find(b)
    if ra != rb: parent[ra] = rb
for ts in FT:
    for j in range(1, len(ts)): union(ts[0], ts[j])
par = defaultdict(int)
for ts in FT: par[find(ts[0])] += 1
ti = sorted(par.values(), reverse=True)
print('ILOTS UV : %d | les 6 plus gros : %s | < 5 faces : %d (%.1f %%)'
      % (len(ti), ti[:6], sum(1 for x in ti if x < 5), 100.0*sum(1 for x in ti if x<5)/max(1,len(ti))))

ar = defaultdict(list)
for vs, ts in zip(F, FT):
    n = len(vs)
    for j in range(n):
        a, b = vs[j], vs[(j+1)%n]
        ar[(min(a,b), max(a,b))].append((ts[j], ts[(j+1)%n], a))
manif = cout = 0
for cle, occ in ar.items():
    if len(occ) != 2: continue
    manif += 1
    (u1,v1,a1), (u2,v2,a2) = occ
    s1 = (u1,v1) if a1 == cle[0] else (v1,u1)
    s2 = (u2,v2) if a2 == cle[0] else (v2,u2)
    if (tuple(uv[s1[0]]), tuple(uv[s1[1]])) != (tuple(uv[s2[0]]), tuple(uv[s2[1]])): cout += 1
print('aretes manifold : %d | COUTURES : %d (%.1f %%)' % (manif, cout, 100.0*cout/max(1,manif)))

r3, r2 = [], []
for vs, ts in zip(F, FT):
    for j in range(1, len(vs)-1):
        p0,p1,p2 = V[vs[0]], V[vs[j]], V[vs[j+1]]
        a3 = 0.5*np.linalg.norm(np.cross(p1-p0, p2-p0))
        q0,q1,q2 = uv[ts[0]], uv[ts[j]], uv[ts[j+1]]
        d1, d2 = q1-q0, q2-q0
        a2 = 0.5*abs(d1[0]*d2[1] - d1[1]*d2[0])
        if a3 > 1e-12 and a2 > 1e-14: r3.append(a3); r2.append(a2)
if not r3:
    print('   -> aucune face d aire exploitable.'); sys.exit(0)
e = np.sqrt(np.array(r3)/np.array(r2)); e = e/np.median(e); e = np.maximum(e, 1.0/e)
print('DISTORSION : mediane %.3f | p90 %.3f | p99 %.3f | MAX %.3f | >1,732 : %.1f %%'
      % (np.median(e), np.percentile(e,90), np.percentile(e,99), e.max(), 100.0*(e>1.732).mean()))
