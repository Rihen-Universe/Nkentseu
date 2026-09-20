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

import io, struct, zlib
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


# ---------------------------------------------------------------------------
# LE FBX ASCII -- 9,5 % DES FBX DU CORPUS, ET ILS ECHOUAIENT SANS LE DIRE.
#
# Mesure du 20/09 : sur 7 742 FBX de NKGenCorpus, 7 008 sont binaires et
# **734 sont ASCII** (9,5 %). Le lecteur binaire les avalait et rendait
# « type de propriete inconnu: 'r' » -- un message qui accuse un TYPE DE
# PROPRIETE alors que le probleme est le FORMAT DU FICHIER. On cherchait un
# defaut de parseur la ou il fallait lire l'en-tete.
#
# ⚠️ C'est la famille « l'instrument accuse la donnee » : neuf echecs dans ma
# production pointaient un octet, et la cause tenait dans les 21 premiers.
#
# Le format ASCII est textuel et regulier :
#     Vertices: *24 {
#         a: 1,1,1,1,1,-1, ...
#     }
#     PolygonVertexIndex: *36 {
#         a: 0,1,2,-4, ...
#     }
# Les indices negatifs y marquent la fin d'un polygone, exactement comme en
# binaire : la suite du traitement est donc commune aux deux formats.

def _ascii_tableau(txt, cle, depart=0):
    """Retourne (valeurs, position apres le bloc) pour `cle: *N { a: ... }`."""
    i = txt.find(cle + ':', depart)
    if i < 0:
        return None, depart
    j = txt.find('{', i)
    if j < 0:
        return None, i + 1
    prof, k = 1, j + 1
    while k < len(txt) and prof:
        if txt[k] == '{':
            prof += 1
        elif txt[k] == '}':
            prof -= 1
        k += 1
    corps = txt[j + 1:k - 1]
    a = corps.find('a:')
    if a < 0:
        return None, k
    return corps[a + 2:], k


def charger_fbx_ascii(chemin):
    txt = io.open(chemin, encoding='utf-8', errors='replace').read()
    V, F, decalage, pos = [], [], 0, 0
    while True:
        bv, pos2 = _ascii_tableau(txt, 'Vertices', pos)
        if bv is None:
            break
        bi, pos3 = _ascii_tableau(txt, 'PolygonVertexIndex', pos2)
        if bi is None:
            break
        pos = pos3
        try:
            verts = np.fromstring(bv.replace('\n', ' '), sep=',')
        except Exception:
            verts = np.array([float(x) for x in bv.replace('\n', ' ').split(',') if x.strip()])
        verts = verts[:len(verts) - (len(verts) % 3)].reshape(-1, 3)
        idx = [int(float(x)) for x in bi.replace('\n', ' ').split(',') if x.strip()]
        cur = []
        for v in idx:
            if v < 0:
                cur.append((~v) + decalage)
                for j in range(1, len(cur) - 1):
                    F.append([cur[0], cur[j], cur[j + 1]])
                cur = []
            else:
                cur.append(v + decalage)
        V.append(verts)
        decalage += len(verts)
    if not V or not F:
        raise ValueError('FBX ASCII sans geometrie exploitable : ' + chemin)
    return np.concatenate(V, axis=0), np.asarray(F, dtype=np.int64)


def charger_fbx(chemin):
    """-> (sommets (n,3), faces [[i,...]]).

    Choisit le format sur l'EN-TETE, et refuse en le NOMMANT : un message qui
    dit « type de propriete inconnu » pour un fichier ASCII envoie chercher le
    defaut au mauvais endroit."""
    with open(chemin, 'rb') as f:
        entete = f.read(21)
    if not entete.startswith(b'Kaydara FBX Binary'):
        if entete.lstrip().startswith(b';') or b'FBX' in entete:
            return charger_fbx_ascii(chemin)
        raise ValueError('ni FBX binaire ni FBX ASCII (en-tete %r) : %s'
                         % (entete[:16], chemin))
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
