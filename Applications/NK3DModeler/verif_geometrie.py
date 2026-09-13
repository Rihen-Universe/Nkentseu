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

EPS = 1e-4


def geoms(path):
    d = json.load(open(path, encoding="utf-8"))
    out = []
    for n in d.get("noeuds", []):
        g = n.get("geometrie")
        if not g:
            continue
        v = base64.b64decode(g["v"])
        i = base64.b64decode(g.get("i", "")) if g.get("i") else b""
        out.append({
            "nom": n.get("nom", ""),
            "stride": g["octetsParSommet"],
            "nv": g["sommets"],
            "ni": g["indices"],
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
              "ecart max %.9f | indices identiques %s | octets identiques %s | %s"
              % (k, x["nom"], x["nv"], y["nv"], x["ni"], y["ni"], worst,
                 "oui" if idxok else "NON", "oui" if octets else "non",
                 "VERT" if ok else "ROUGE"))
        if not ok:
            rouges += 1
    print("VERDICT : %s (%d bloc(s) rouge(s))" % ("VERT" if rouges == 0 else "ROUGE", rouges))
    return rouges


if __name__ == "__main__":
    sys.exit(main())
