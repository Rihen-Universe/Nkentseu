# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# =============================================================================
# verif_geometrie.py — compare la GEOMETRIE de deux fichiers d'asset
# NK3DModeler (.nkmesh / .nkscene).
#
# CE QU'IL MESURE, ET POURQUOI IL EXISTE A COTE DE `--probe=geom`.
#   La sonde interne eprouve le CODEC, sans fenetre. Celui-ci eprouve l'ALLER-
#   RETOUR PAR L'APPLICATION : on enregistre, on relance un PROCESSUS NEUF, on
#   re-enregistre, et on compare les deux fichiers. Ce que le second porte a ete
#   relu du disque, remis dans la vue 3D, puis RE-lu de la vue 3D -- rien n'y
#   survit de la session qui a ecrit le premier.
#
# Critere : memes comptes (sommets, indices), memes indices, et ecart max sur
# les POSITIONS < 1e-4 (l'identite spatiale de la maison).
#
# VOLET NEGATIF, obligatoire : `--bouge <i>` deplace le sommet i de 0,01 dans le
# PREMIER fichier avant de comparer. La comparaison DOIT alors echouer -- sinon
# « ecart max 0 » ne prouve que l'immobilite de l'instrument.
#
# Usage :
#   python verif_geometrie.py A.nkmesh B.nkmesh
#   python verif_geometrie.py A.nkmesh B.nkmesh --bouge 0
# Code de sortie : 0 si tout est vert, sinon le nombre de blocs rouges.
# =============================================================================
import base64
import json
import struct
import sys
import zlib

EPS = 1e-4


def unrle(data, expect):
    """RLE du bloc : 0x00,compte,valeur = serie ; L,<L octets> = litteraux."""
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        tag = data[i]
        i += 1
        if tag == 0:
            if i + 1 >= n:
                raise ValueError("RLE tronque")
            run, val = data[i], data[i + 1]
            i += 2
            out += bytes([val]) * run
        else:
            out += data[i:i + tag]
            i += tag
    if len(out) != expect:
        raise ValueError("RLE : %d octets au lieu de %d" % (len(out), expect))
    return bytes(out)


def untranspose(data, stride):
    if stride <= 1:
        return data
    rows = len(data) // stride
    out = bytearray(len(data))
    k = 0
    for c in range(stride):
        for r in range(rows):
            out[r * stride + c] = data[k]
            k += 1
    for i in range(rows * stride, len(data)):
        out[i] = data[k]
        k += 1
    return bytes(out)


def decode(g, key, codage, stride):
    """Les trois codages du bloc `geometrie`. Un codage inconnu est refuse,
    jamais devine -- meme regle que le lecteur C++."""
    raw = base64.b64decode(g[key])
    brut = g.get(key + "Brut", 0)
    if codage in ("", "brut", None):
        return raw
    if codage == "transpose-rle":
        return untranspose(unrle(raw, brut), stride)
    if codage == "transpose-deflate":
        d = zlib.decompress(raw)
        if len(d) != brut:
            raise ValueError("deflate : %d octets au lieu de %d" % (len(d), brut))
        return untranspose(d, stride)
    raise ValueError("codage inconnu : %r" % codage)


def geoms(path):
    d = json.load(open(path, encoding="utf-8"))
    out = []
    for n in d.get("noeuds", []):
        g = n.get("geometrie")
        if not g:
            continue
        st = g["octetsParSommet"]
        v = decode(g, "v", g.get("codage", "brut"), st)
        i = decode(g, "i", g.get("codageIndices", "brut"), 4) if g.get("i") else b""
        out.append({
            "nom": n.get("nom", ""),
            "stride": st,
            "nv": g["sommets"],
            "ni": g["indices"],
            "codage": g.get("codage", "brut"),
            "v": v,
            "i": i,
        })
    return out


def positions(g):
    s, n = g["stride"], g["nv"]
    return [struct.unpack_from("<3f", g["v"], k * s) for k in range(n)]


def main():
    a, b = sys.argv[1], sys.argv[2]
    bouge = -1
    if "--bouge" in sys.argv:
        bouge = int(sys.argv[sys.argv.index("--bouge") + 1])
    ga, gb = geoms(a), geoms(b)
    print("A %s : %d bloc(s) de geometrie" % (a, len(ga)))
    print("B %s : %d bloc(s) de geometrie" % (b, len(gb)))
    if len(ga) != len(gb) or not ga:
        print("VERDICT : ROUGE (nombre de blocs different, ou aucun bloc)")
        return 1
    rouges = 0
    for k, (x, y) in enumerate(zip(ga, gb)):
        pa, pb = positions(x), positions(y)
        if bouge >= 0 and bouge < len(pa):
            p = list(pa[bouge])
            p[0] += 0.01
            pa[bouge] = tuple(p)
        okc = (x["nv"] == y["nv"] and x["ni"] == y["ni"] and x["stride"] == y["stride"])
        worst = -1.0
        if okc:
            worst = 0.0
            for u, w in zip(pa, pb):
                for c in range(3):
                    worst = max(worst, abs(u[c] - w[c]))
        idxok = x["i"] == y["i"]
        octets = (x["v"] == y["v"])
        ok = okc and idxok and 0.0 <= worst < EPS
        print("  bloc %d (%s) : sommets %d -> %d | indices %d -> %d | "
              "codage %s -> %s | ecart max %.9f | indices identiques %s | "
              "octets identiques %s | %s"
              % (k, x["nom"], x["nv"], y["nv"], x["ni"], y["ni"],
                 x["codage"], y["codage"], worst,
                 "oui" if idxok else "NON", "oui" if octets else "non",
                 "VERT" if ok else "ROUGE"))
        if not ok:
            rouges += 1
    print("VERDICT : %s (%d bloc(s) rouge(s))" % ("VERT" if rouges == 0 else "ROUGE", rouges))
    return rouges


if __name__ == "__main__":
    sys.exit(main())
