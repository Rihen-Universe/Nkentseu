# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
# Lit un FBX binaire 7400 et mesure SES UV : ilots, coutures, distorsion.
# On mesure un etalon COMMERCIAL avec le MEME instrument que notre box projection.
#
# ATTENDU ECRIT AVANT LA MESURE (derive, pas devine) :
#   A1. Un atlas professionnel vise PEU d'ilots. Notre box projection en produit 272
#       dont 75,7 % de moins de 5 faces. Si Meshy est un atlas soigne, j'attends
#       un nombre d'ilots de l'ordre de la DIZAINE, pas de la centaine.
#   A2. Coutures : nettement SOUS nos 34,3 % des aretes.
#   A3. Distorsion : c'est le point interessant. Un depliage conforme (LSCM) accepte
#       de l'ETIREMENT pour eviter des coutures. Donc j'attends une distorsion MAX
#       SUPERIEURE a notre borne 1,732, avec une mediane proche de 1.
#   -> SI A3 se verifie, mon critere de depliage etait MAL PONDERE : l'industrie
#      paie en distorsion pour acheter de la continuite. Ce serait la lecon.
#
# NEGATIF : on melange les UV au hasard et on verifie que les trois mesures bougent.

import sys, struct, zlib
import numpy as np

CHEMIN = sys.argv[1]

# ---------------------------------------------------------------- lecteur FBX
class Noeud:
    def __init__(s, nom):
        s.nom = nom; s.props = []; s.enfants = []
    def trouver(s, nom):
        for e in s.enfants:
            if e.nom == nom: return e
        return None
    def tous(s, nom):
        return [e for e in s.enfants if e.nom == nom]

def lire_prop(f):
    t = f.read(1).decode('ascii')
    if t == 'Y': return struct.unpack('<h', f.read(2))[0]
    if t == 'C': return struct.unpack('<?', f.read(1))[0]
    if t == 'I': return struct.unpack('<i', f.read(4))[0]
    if t == 'F': return struct.unpack('<f', f.read(4))[0]
    if t == 'D': return struct.unpack('<d', f.read(8))[0]
    if t == 'L': return struct.unpack('<q', f.read(8))[0]
    if t in 'fdlib':
        n, enc, clen = struct.unpack('<III', f.read(12))
        brut = f.read(clen)
        if enc == 1: brut = zlib.decompress(brut)
        dt = {'f':'<f4','d':'<f8','l':'<i8','i':'<i4','b':'|i1'}[t]
        return np.frombuffer(brut, dtype=dt, count=n)
    if t in 'SR':
        n = struct.unpack('<I', f.read(4))[0]
        return f.read(n)
    raise ValueError('type de propriete inconnu: ' + repr(t))

def lire_noeud(f, v):
    large = v >= 7500
    taille = 8 if large else 4
    fmt = '<QQQ' if large else '<III'
    entete = f.read(taille * 3)
    if len(entete) < taille * 3: return None
    fin, nprops, _plen = struct.unpack(fmt, entete)
    nlen = struct.unpack('<B', f.read(1))[0]
    nom = f.read(nlen).decode('utf-8', 'replace')
    if fin == 0: return None
    n = Noeud(nom)
    for _ in range(nprops): n.props.append(lire_prop(f))
    # enfants
    sentinelle = 25 if large else 13
    while f.tell() + sentinelle <= fin:
        e = lire_noeud(f, v)
        if e is None: break
        n.enfants.append(e)
    f.seek(fin)
    return n

with open(CHEMIN, 'rb') as f:
    f.read(23)
    version = struct.unpack('<I', f.read(4))[0]
    racine = Noeud('__racine__')
    while True:
        n = lire_noeud(f, version)
        if n is None: break
        racine.enfants.append(n)

print('FBX version', version)

objets = racine.trouver('Objects')
geos = objets.tous('Geometry') if objets else []
print('Geometry trouvees :', len(geos))

g = geos[0]
verts = g.trouver('Vertices').props[0].reshape(-1, 3)
pvi = g.trouver('PolygonVertexIndex').props[0]
uvl = g.trouver('LayerElementUV')
uv = uvl.trouver('UV').props[0].reshape(-1, 2)
uvi_n = uvl.trouver('UVIndex')
uvi = uvi_n.props[0] if uvi_n else None
mapping = uvl.trouver('MappingInformationType').props[0].decode()
refer = uvl.trouver('ReferenceInformationType').props[0].decode()

print('sommets 3D :', len(verts), '| coins :', len(pvi), '| UV distinctes :', len(uv))
print('mapping :', mapping, '| reference :', refer)

# ------------------------------------------------- faces depuis PolygonVertexIndex
faces, faces_coins, cur, cur_c = [], [], [], []
for k, idx in enumerate(pvi):
    if idx < 0:
        cur.append(~idx); cur_c.append(k)
        faces.append(cur); faces_coins.append(cur_c); cur, cur_c = [], []
    else:
        cur.append(idx); cur_c.append(k)

tailles = np.array([len(x) for x in faces])
print('faces :', len(faces), '| triangles :', int((tailles == 3).sum()),
      '| quads :', int((tailles == 4).sum()), '| n-gons :', int((tailles > 4).sum()))

# indice UV par coin
if uvi is not None:
    coin_uv = np.asarray(uvi)
else:
    coin_uv = np.arange(len(pvi))

def mesurer(coin_uv, etiquette):
    print('')
    print('=== ' + etiquette + ' ===')
    # --- ilots : composantes connexes sur les indices UV partages
    parent = list(range(len(uv)))
    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]; a = parent[a]
        return a
    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb: parent[ra] = rb
    for cs in faces_coins:
        for j in range(1, len(cs)):
            union(int(coin_uv[cs[0]]), int(coin_uv[cs[j]]))
    # ilots comptes en FACES
    from collections import defaultdict
    par_ilot = defaultdict(int)
    for cs in faces_coins:
        par_ilot[find(int(coin_uv[cs[0]]))] += 1
    tailles_i = sorted(par_ilot.values(), reverse=True)
    print('ILOTS UV :', len(tailles_i))
    print('  les 6 plus gros :', tailles_i[:6])
    petits = sum(1 for t in tailles_i if t < 5)
    print('  ilots de moins de 5 faces :', petits,
          '(%.1f %%)' % (100.0 * petits / max(1, len(tailles_i))))

    # --- coutures : arete 3D partagee par 2 faces mais UV differentes
    aretes = defaultdict(list)
    for fi, (vs, cs) in enumerate(zip(faces, faces_coins)):
        for j in range(len(vs)):
            a, b = vs[j], vs[(j + 1) % len(vs)]
            ca, cb = cs[j], cs[(j + 1) % len(cs)]
            cle = (min(a, b), max(a, b))
            aretes[cle].append((int(coin_uv[ca]), int(coin_uv[cb]), a, b))
    manif = coutures = 0
    for cle, occ in aretes.items():
        if len(occ) != 2: continue
        manif += 1
        (u1, v1, a1, _), (u2, v2, a2, _) = occ
        s1 = (u1, v1) if a1 == cle[0] else (v1, u1)
        s2 = (u2, v2) if a2 == cle[0] else (v2, u2)
        p1 = (tuple(uv[s1[0]]), tuple(uv[s1[1]]))
        p2 = (tuple(uv[s2[0]]), tuple(uv[s2[1]]))
        if p1 != p2: coutures += 1
    print('aretes manifold :', manif)
    print('COUTURES :', coutures, '(%.1f %%)' % (100.0 * coutures / max(1, manif)))

    # --- distorsion : rapport aire3D / aireUV, normalise par la mediane
    r3, r2 = [], []
    for vs, cs in zip(faces, faces_coins):
        for j in range(1, len(vs) - 1):
            p0, p1_, p2_ = verts[vs[0]], verts[vs[j]], verts[vs[j + 1]]
            a3 = 0.5 * np.linalg.norm(np.cross(p1_ - p0, p2_ - p0))
            q0 = uv[int(coin_uv[cs[0]])]; q1 = uv[int(coin_uv[cs[j]])]; q2 = uv[int(coin_uv[cs[j + 1]])]
            d1 = q1 - q0; d2 = q2 - q0; a2 = 0.5 * abs(d1[0]*d2[1] - d1[1]*d2[0])
            if a3 > 1e-12 and a2 > 1e-14:
                r3.append(a3); r2.append(a2)
    r3 = np.array(r3); r2 = np.array(r2)
    ech = np.sqrt(r3 / r2)
    ech = ech / np.median(ech)          # normalise : seule la VARIATION compte
    ech = np.maximum(ech, 1.0 / ech)    # facteur d'etirement, toujours >= 1
    print('triangles mesures :', len(ech))
    print('DISTORSION (etirement relatif, 1 = parfait)')
    print('  mediane : %.3f | moyenne : %.3f' % (np.median(ech), ech.mean()))
    print('  p90 : %.3f | p99 : %.3f | MAX : %.3f'
          % (np.percentile(ech, 90), np.percentile(ech, 99), ech.max()))
    print('  au-dela de 1,732 (notre borne 6 axes) : %d (%.1f %%)'
          % (int((ech > 1.732).sum()), 100.0 * (ech > 1.732).mean()))
    return len(tailles_i), coutures, float(np.median(ech)), float(ech.max())

vrai = mesurer(coin_uv, 'ETALON MESHY, SES VRAIES UV')

# ------------------------------------------------------------------- LE NEGATIF
rng = np.random.default_rng(12345)
# CORRECTION : une PERMUTATION est bijective, donc elle preserve exactement la
# structure d'incidence (ilots, coutures) -> elle ne peut RIEN refuter sur ces deux
# criteres. Le vrai negatif casse l'incidence : chaque coin recoit un indice TIRE
# INDEPENDAMMENT. Un atlas ainsi detruit doit exploser les ilots ET les coutures.
alea = rng.integers(0, len(uv), size=len(coin_uv))
faux = mesurer(alea, 'NEGATIF CORRIGE : chaque coin recoit un indice UV INDEPENDANT')

print('')
print('=== LE NEGATIF A-T-IL BOUGE ? ===')
noms = ['ilots', 'coutures', 'distorsion mediane', 'distorsion max']
for n, a, b in zip(noms, vrai, faux):
    verdict = 'BOUGE' if abs(a - b) > 1e-9 else '*** MUET : critere aveugle ***'
    print('  %-20s vrai=%-12s melange=%-12s %s' % (n, round(a, 3), round(b, 3), verdict))
