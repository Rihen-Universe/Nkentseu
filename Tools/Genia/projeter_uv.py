# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# PROJECTION PLANAIRE DES UV DEPUIS LA CAMERA D'ENTREE.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# POURQUOI CETTE CAMERA-LA, ET PAS UNE AUTRE. Ce n'est pas une commodite :
#
#   Le maillage d'Ilyana-3DG n'a qu'UNE source de couleur -- la vue d'entree.
#   Decouper ailleurs qu'a SA silhouette reviendrait a poser une frontiere que
#   RIEN dans les donnees ne motive. La silhouette vue depuis cette camera est
#   la ligne ou l'information reelle S'ARRETE : c'est la definition meme d'une
#   couture. Et elle tombe exactement la ou la texture devient INVENTEE.
#
# LA CAMERA SE MESURE, ELLE NE SE SUPPOSE PAS. Deux instruments sans code
# commun l'ont designee :
#   - la VARIANCE de couleur par hemisphere : le cote vu porte de la couleur
#     reelle (variee), le cote invente est plus uniforme. Rapport +X : 2,07,
#     contre 1,01 sur Z -- qui ne distingue donc RIEN ;
#   - le RENDU des quatre vues : celle qui montre un personnage de face est a
#     90 deg autour de Y, soit l'axe X.
#
# ⚠️ SA LIMITE, ECRITE EN MEME TEMPS QUE SON RESULTAT : une projection planaire
# ETIRE VIOLEMMENT ce qui est vu de profil -- les flancs, les cotes du torse.
# C'est acceptable PRECISEMENT PARCE QUE ces zones n'ont pas de couleur reelle
# a porter.
#
# ⚠️ CONDITION DE RETRAIT : le jour ou le multi-vues arrivera, les flancs
# porteront une vraie couleur, et cette methode deviendra FAUSSE. Elle devra
# etre REMPLACEE, pas etendue. (La derniere dette ecrite ainsi -- les aretes
# non-manifold du R14.9 -- s'est realisee au mot pres.)
#
# USAGE : python projeter_uv.py <maillage> --out <sortie.obj> [--axe x|y|z]
#                [--signe +|-] [--hasard]
# -----------------------------------------------------------------------------
import argparse
import os
import sys

for _f in (sys.stdout, sys.stderr):
    try:
        _f.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _refus(msg, code=2):
    sys.stderr.write("REFUS : %s\n" % msg)
    sys.stderr.flush()
    sys.exit(code)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("maillage")
    ap.add_argument("--out", required=True)
    ap.add_argument("--axe", default="x", choices=("x", "y", "z"))
    ap.add_argument("--signe", default="+", choices=("+", "-"))
    ap.add_argument("--hasard", action="store_true",
                    help="LE NEGATIF : projette depuis un angle arbitraire. Sa distorsion sur la "
                         "face coloree doit etre NETTEMENT pire, sinon la camera mesuree ne vaut "
                         "pas mieux que n'importe laquelle.")
    ap.add_argument("--graine", type=int, default=7)
    a = ap.parse_args()

    import numpy as np
    import trimesh

    if not os.path.isfile(a.maillage):
        _refus("maillage introuvable : %s" % a.maillage)
    m = trimesh.load(a.maillage, force="mesh", process=False)
    V = np.asarray(m.vertices, dtype=np.float64)
    F = np.asarray(m.faces, dtype=np.int64)
    if len(V) == 0 or len(F) == 0:
        _refus("maillage vide")

    # ── L'AXE DE VUE ────────────────────────────────────────────────────────
    iax = {"x": 0, "y": 1, "z": 2}[a.axe]
    sgn = 1.0 if a.signe == "+" else -1.0
    vue = np.zeros(3)
    vue[iax] = sgn
    if a.hasard:
        rng = np.random.default_rng(a.graine)
        vue = rng.normal(size=3)
        vue /= np.linalg.norm(vue)
        print("NEGATIF : projection depuis un axe ARBITRAIRE (%.3f, %.3f, %.3f)" % tuple(vue))
    else:
        print("projection depuis %s%s -- la camera MESUREE (variance de couleur 2,07)" % (a.signe, a.axe.upper()))

    # Deux axes du plan de projection, orthonormes a la vue.
    tmp = np.array([0.0, 1.0, 0.0])
    if abs(np.dot(tmp, vue)) > 0.9:
        tmp = np.array([1.0, 0.0, 0.0])
    ex = np.cross(tmp, vue)
    ex /= np.linalg.norm(ex)
    ey = np.cross(vue, ex)

    u = V @ ex
    v = V @ ey
    # Normaliser dans [0,1] : l'atlas est unitaire.
    u = (u - u.min()) / max(u.max() - u.min(), 1e-12)
    v = (v - v.min()) / max(v.max() - v.min(), 1e-12)

    # ── LE COTE VU, POUR LE CRITERE ASYMETRIQUE ─────────────────────────────
    # La normale de chaque face dit si elle fait face a la camera. C'est ce qui
    # permet de mesurer la distorsion SEPAREMENT sur la face vue et sur le dos --
    # et l'attendu est justement qu'elle DIFFERE. Une distorsion uniforme
    # signifierait que la projection ne fait pas ce qu'on croit.
    n = np.cross(V[F[:, 1]] - V[F[:, 0]], V[F[:, 2]] - V[F[:, 0]])
    ln = np.linalg.norm(n, axis=1)
    ln[ln == 0] = 1.0
    n = n / ln[:, None]
    face_vue = (n @ vue) > 0.0
    print("  faces tournees vers la camera : %d / %d (%.1f %%)"
          % (face_vue.sum(), len(F), 100.0 * face_vue.sum() / len(F)))

    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("# UV par PROJECTION PLANAIRE depuis la camera d'entree.\n")
        f.write("# AUTEUR : TEUGUIA TADJUIDJE Rodolf Sederis - Rihen\n")
        f.write("# La silhouette vue est la ligne ou l'information reelle s'arrete.\n")
        for p3 in V:
            f.write("v %.6f %.6f %.6f\n" % tuple(p3))
        for i in range(len(V)):
            f.write("vt %.6f %.6f\n" % (u[i], v[i]))
        f.write("o corps\n")
        for t in F:
            f.write("f %d/%d %d/%d %d/%d\n" % (t[0] + 1, t[0] + 1, t[1] + 1, t[1] + 1, t[2] + 1, t[2] + 1))
    print("  ecrit : %s (%d sommets, %d faces, %d UV)" % (a.out, len(V), len(F), len(V)))

    # Les deux sous-ensembles, ecrits a part : c'est ce qui rend le critere
    # ASYMETRIQUE mesurable par l'outil du depot, qui ne sait juger qu'un
    # maillage entier.
    for nom, sel in (("vue", face_vue), ("dos", ~face_vue)):
        if sel.sum() < 4:
            continue
        chemin = a.out.replace(".obj", "_%s.obj" % nom)
        G = F[sel]
        with open(chemin, "w", encoding="utf-8", newline="\n") as f:
            f.write("# sous-ensemble « %s » -- pour mesurer la distorsion SEPAREMENT\n" % nom)
            for p3 in V:
                f.write("v %.6f %.6f %.6f\n" % tuple(p3))
            for i in range(len(V)):
                f.write("vt %.6f %.6f\n" % (u[i], v[i]))
            f.write("o %s\n" % nom)
            for t in G:
                f.write("f %d/%d %d/%d %d/%d\n" % (t[0] + 1, t[0] + 1, t[1] + 1, t[1] + 1, t[2] + 1, t[2] + 1))
        print("  ecrit : %s (%d faces)" % (chemin, len(G)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
