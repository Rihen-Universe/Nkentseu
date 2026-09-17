# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# APERCU -- rendu PROCESSEUR PUR d'un maillage (.obj / .glb / .gltf) en PNG.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# POURQUOI IL EXISTE : « les images trouvent ce que les nombres validaient ».
# Un maillage peut avoir la bonne caracteristique d'Euler, le bon nombre de
# sommets, etre etanche -- et etre une forme que personne ne reconnait. Aucun
# chiffre ne voit cela.
#
# POURQUOI IL N'UTILISE PAS LA CARTE GRAPHIQUE : la RTX 3070 de Rodolf (8 192
# Mio) est reservee a l'entrainement d'Ilyana. Ce script est un rasteriseur
# numpy, avec tampon de profondeur et ombrage de Lambert. Il ne touche RIEN.
#
# CE QU'IL NE FAIT PAS : pas de textures, pas d'ombres, pas d'anticrenelage
# autre que le sur-echantillonnage. C'est un APERCU, et il le dit.
#
# USAGE : python apercu_maillage.py <maillage> --out <image.png> [--taille 480]
# -----------------------------------------------------------------------------
import argparse
import os
import sys


def _refus(msg, code=2):
    sys.stderr.write("REFUS : %s\n" % msg)
    sys.stderr.flush()
    sys.exit(code)


def charger(path):
    import numpy as np
    import trimesh
    m = trimesh.load(path, force="mesh", process=False)
    if not hasattr(m, "vertices") or len(m.vertices) == 0:
        _refus("aucun sommet lu dans %s" % path)
    V = np.asarray(m.vertices, dtype=np.float64)
    F = np.asarray(m.faces, dtype=np.int64)
    C = None
    try:
        if getattr(m.visual, "kind", None) == "vertex":
            C = np.asarray(m.visual.vertex_colors, dtype=np.float64)[:, :3] / 255.0
    except Exception:
        C = None
    return V, F, C


def rendre(V, F, C, taille, angleDeg, plongee):
    """Une vue : rotation autour de Y, projection orthographique, z-buffer."""
    import numpy as np
    a = np.radians(angleDeg)
    ca, sa = np.cos(a), np.sin(a)
    R = np.array([[ca, 0.0, sa], [0.0, 1.0, 0.0], [-sa, 0.0, ca]])
    # LA PLONGEE EST UN PARAMETRE, et c'est une mesure du 17/09 qui l'a impose :
    # a 20 degres, un TORE parait un DISQUE PLEIN -- la paroi interieure du cote
    # oppose bouche exactement le trou. Le maillage etait juste (chi = 0, rien en
    # deca de 42 % du rayon), c'est le POINT DE VUE qui mentait. Une vue rasante
    # ne peut pas montrer une anse verticale.
    b = np.radians(plongee)
    cb, sb = np.cos(b), np.sin(b)
    Rx = np.array([[1.0, 0.0, 0.0], [0.0, cb, -sb], [0.0, sb, cb]])
    P = (V @ R.T) @ Rx.T

    mn, mx = P.min(axis=0), P.max(axis=0)
    ctr = 0.5 * (mn + mx)
    ech = float(max(mx[0] - mn[0], mx[1] - mn[1]))
    if ech <= 0.0:
        ech = 1.0
    marge = 0.90
    sx = (P[:, 0] - ctr[0]) / ech * taille * marge + taille * 0.5
    sy = taille * 0.5 - (P[:, 1] - ctr[1]) / ech * taille * marge
    sz = P[:, 2]

    # Normale de face dans l'espace de la vue -> Lambert, lumiere au-dessus a
    # gauche, plus un terme ambiant : sans ambiant, la face arriere est NOIRE et
    # indiscernable du fond, donc du vide.
    A = P[F[:, 0]]
    B = P[F[:, 1]]
    Cc = P[F[:, 2]]
    N = np.cross(B - A, Cc - A)
    n = np.linalg.norm(N, axis=1)
    n[n == 0.0] = 1.0
    N = N / n[:, None]
    L = np.array([-0.45, 0.55, 0.70])
    L = L / np.linalg.norm(L)
    lam = np.clip(N @ L, 0.0, 1.0)
    inten = 0.25 + 0.75 * lam

    img = np.zeros((taille, taille, 3), dtype=np.float64)
    img[:, :] = (0.10, 0.10, 0.12)  # un fond que RIEN d'autre ne produit
    zbuf = np.full((taille, taille), -1e30)

    ax, ay, az = sx[F[:, 0]], sy[F[:, 0]], sz[F[:, 0]]
    bx, by, bz = sx[F[:, 1]], sy[F[:, 1]], sz[F[:, 1]]
    cx, cy, cz = sx[F[:, 2]], sy[F[:, 2]], sz[F[:, 2]]
    ordre = np.argsort((az + bz + cz) / 3.0)  # peintre, puis z-buffer par pixel

    for k in ordre:
        x0 = int(max(0, np.floor(min(ax[k], bx[k], cx[k]))))
        x1 = int(min(taille - 1, np.ceil(max(ax[k], bx[k], cx[k]))))
        y0 = int(max(0, np.floor(min(ay[k], by[k], cy[k]))))
        y1 = int(min(taille - 1, np.ceil(max(ay[k], by[k], cy[k]))))
        if x1 < x0 or y1 < y0:
            continue
        xs = np.arange(x0, x1 + 1) + 0.5
        ys = np.arange(y0, y1 + 1) + 0.5
        gx, gy = np.meshgrid(xs, ys)
        d = (by[k] - cy[k]) * (ax[k] - cx[k]) + (cx[k] - bx[k]) * (ay[k] - cy[k])
        if abs(d) < 1e-12:
            continue
        w0 = ((by[k] - cy[k]) * (gx - cx[k]) + (cx[k] - bx[k]) * (gy - cy[k])) / d
        w1 = ((cy[k] - ay[k]) * (gx - cx[k]) + (ax[k] - cx[k]) * (gy - cy[k])) / d
        w2 = 1.0 - w0 - w1
        dans = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not dans.any():
            continue
        z = w0 * az[k] + w1 * bz[k] + w2 * cz[k]
        sousImg = zbuf[y0:y1 + 1, x0:x1 + 1]
        garde = dans & (z > sousImg)
        if not garde.any():
            continue
        sousImg[garde] = z[garde]
        if C is not None:
            col = (C[F[k, 0]] + C[F[k, 1]] + C[F[k, 2]]) / 3.0
        else:
            col = np.array([0.97, 0.60, 0.16])  # orange Rihen #F79A28
        img[y0:y1 + 1, x0:x1 + 1][garde] = np.clip(col * inten[k], 0.0, 1.0)
    return img


def main():
    ap = argparse.ArgumentParser(description="Apercu CPU d'un maillage")
    ap.add_argument("maillage")
    ap.add_argument("--out", required=True)
    ap.add_argument("--taille", type=int, default=420)
    ap.add_argument("--vues", type=int, default=3)
    a = ap.parse_args()
    if not os.path.isfile(a.maillage):
        _refus("maillage introuvable : %s" % a.maillage)
    if not a.out.lower().endswith(".png"):
        _refus("la sortie doit etre un .png : %s" % a.out)
    if a.taille < 64 or a.taille > 2048:
        _refus("taille hors bornes : %d (64..2048)" % a.taille)

    import numpy as np
    from PIL import Image
    V, F, C = charger(a.maillage)
    print("MESURE apercu : %s -> sommets=%d triangles=%d couleurs=%s"
          % (os.path.basename(a.maillage), len(V), len(F), "sommets" if C is not None else "aucune"))

    # LES ANGLES NE SONT PAS LIBRES. 25/115/205 etaient espaces de 90 et 180
    # degres : sur un CUBE, les trois vues sortaient IDENTIQUES par symetrie --
    # un triptyque qui ne distingue rien. On choisit donc des ecarts qui ne sont
    # multiples ni de 90 ni de 180, et des PLONGEES differentes, dont une quasi
    # verticale qui montre les anses.
    poses = [(20.0, 18.0), (140.0, 45.0), (260.0, 78.0)][:max(1, a.vues)]
    angles = [p0 for p0, _ in poses]
    vues = [rendre(V, F, C, a.taille, p0, p1) for p0, p1 in poses]
    bande = np.concatenate(vues, axis=1)
    # PROUVER LE ZERO DE L'IMAGE : si aucun pixel ne differe du fond, l'apercu
    # est vide et un « fichier ecrit » serait un faux succes. Un fond noir est
    # indiscernable d'une cible que personne n'a peinte.
    fond = np.array([0.10, 0.10, 0.12])
    peints = int((np.abs(bande - fond).max(axis=2) > 0.02).sum())
    print("MESURE apercu : pixels peints = %d / %d (%.1f %%)"
          % (peints, bande.shape[0] * bande.shape[1],
             100.0 * peints / (bande.shape[0] * bande.shape[1])))
    if peints == 0:
        _refus("apercu VIDE : aucun pixel peint (le maillage ne traverse pas la vue)", 3)

    Image.fromarray((bande * 255.0).astype(np.uint8)).save(a.out)
    print("SORTIE : %s (%d x %d, %d vues)" % (os.path.abspath(a.out), bande.shape[1], bande.shape[0], len(angles)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
