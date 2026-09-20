# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
#
# LECTEUR FBX BINAIRE (versions 7400 et 7500) -- AUTORITE UNIQUE.
#
# POURQUOI IL EXISTE : rien dans le venv ne lit le FBX (pyassimp absent,
# trimesh refuse « file_type 'fbx' not supported »). Or l'inventaire du corpus
# compte 1 159 FBX sur 3 096 modeles : **37,4 %** de la matiere serait
# inaccessible sans lui.
#
# POURQUOI IL EST SEUL : il a d'abord ete ecrit dans `mesurer_uv_fbx.py`. Le
# recopier ailleurs aurait cree un etat duplique sans autorite unique -- la
# famille de defauts la plus chere de ce depot. Les deux consommateurs
# l'importent donc d'ici.
#
# CE QU'IL REND : positions, indices de coins, et les UV si elles existent.

import struct, zlib
import numpy as np


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

def lire_fbx(chemin):
    with open(chemin, 'rb') as f:
        f.read(23)
        version = struct.unpack('<I', f.read(4))[0]
        racine = Noeud('__racine__')
        while True:
            n = lire_noeud(f, version)
            if n is None:
                break
            racine.enfants.append(n)
    return racine


def charger_fbx(chemin):
    """-> (sommets (n,3), faces [[i,...]]) depuis la premiere Geometry."""
    racine = lire_fbx(chemin)
    objets = racine.trouver('Objects')
    if objets is None:
        raise ValueError('FBX sans noeud Objects : ' + chemin)
    geos = objets.tous('Geometry')
    if not geos:
        raise ValueError('FBX sans Geometry : ' + chemin)
    V, F = [], []
    decalage = 0
    for g in geos:
        nv = g.trouver('Vertices')
        npv = g.trouver('PolygonVertexIndex')
        if nv is None or npv is None:
            continue
        verts = np.asarray(nv.props[0], dtype=np.float64).reshape(-1, 3)
        pvi = np.asarray(npv.props[0])
        cur = []
        for idx in pvi:
            if idx < 0:
                cur.append(int(~idx) + decalage)
                # eventail : un polygone de n cotes -> n-2 triangles
                for j in range(1, len(cur) - 1):
                    F.append([cur[0], cur[j], cur[j + 1]])
                cur = []
            else:
                cur.append(int(idx) + decalage)
        V.append(verts)
        decalage += len(verts)
    if not V:
        raise ValueError('FBX sans geometrie exploitable : ' + chemin)
    return np.concatenate(V, axis=0), np.asarray(F, dtype=np.int64)
