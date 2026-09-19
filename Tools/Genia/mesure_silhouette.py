# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# SILHOUETTE -- le SECOND instrument de la reduction de maillage, et il ne
# partage AUCUNE ligne avec le premier.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# POURQUOI IL EXISTE. Le temoin C++ juge la reduction par chi = V - E + F, par
# le compte d'aretes de bord, par le volume signe et par `ShapeError`. Ces quatre
# grandeurs viennent du MEME code, celui de NKRenderer. Si ce code se trompe, les
# quatre se trompent ensemble et s'accordent -- et un accord parfait entre
# instruments qui partagent leur code ne prouve rien. Celui-ci rasterise en numpy
# sur processeur, lit les fichiers par trimesh, et ne connait ni NkEditMesh ni
# NkMeshDecimate.
#
# CE QU'IL MESURE : l'intersection sur union (IoU) des SILHOUETTES du maillage
# d'origine et du maillage allege, sur plusieurs vues. La silhouette est ce que
# l'oeil voit en premier ; c'est aussi la grandeur que Rodolf nomme quand il dit
# « a quel prix visuel ».
#
# ⚠️ LE PIEGE QU'IL EVITE, ET IL EST GROS : `apercu_maillage.py` recentre et
# remet a l'echelle CHAQUE maillage sur sa propre boite. Deux objets de tailles
# differentes y paraissent identiques. Une comparaison faite sur ces images ne
# verrait JAMAIS un retrecissement -- exactement le defaut qu'une decimation sans
# retenue des bords produit. Ici l'echelle et le centre sont ceux de la
# REFERENCE, imposes aux deux.
#
# LA BORNE EST DERIVEE, PAS CHOISIE. Si toute la surface se deplace d'au plus
# delta, la silhouette ne peut changer que dans une bande d'epaisseur delta le
# long de son contour. Donc
#     1 - IoU  <=  Perimetre_px x delta_px / Aire_px
# ou le perimetre et l'aire sont MESURES sur le masque de reference, et delta_px
# se deduit de delta_monde par l'echelle du rendu. Aucun seuil de confort.
#
# LE ZERO SE PROUVE EN PREMIER : la reference comparee a ELLE-MEME doit rendre
# IoU = 1,000000 exactement. Un comparateur qui ne rend pas 1 sur l'identite ne
# mesure rien, et tout ce qui suivrait serait du bruit.
#
# USAGE : python mesure_silhouette.py <reference> <allege> --delta D [--vues 6]
#                                     [--taille 512] [--images DOSSIER]
#         code 0 = sous la borne · 1 = au-dessus · 2 = refus nomme · 3 = le zero
#         a echoue (l'instrument se declare hors service plutot que de mesurer)
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
    if not os.path.isfile(path):
        _refus("fichier introuvable : %s" % path)
    m = trimesh.load(path, force="mesh", process=False)
    if not hasattr(m, "vertices") or len(m.vertices) == 0:
        _refus("aucun sommet lu dans %s" % path)
    return (np.asarray(m.vertices, dtype=np.float64),
            np.asarray(m.faces, dtype=np.int64))


def masque(V, F, taille, angleDeg, plongee, centre, echelle):
    """Silhouette BINAIRE. Le centre et l'echelle sont IMPOSES (ceux de la
    reference) : c'est ce qui rend les deux masques comparables."""
    import numpy as np
    a = np.radians(angleDeg)
    ca, sa = np.cos(a), np.sin(a)
    R = np.array([[ca, 0.0, sa], [0.0, 1.0, 0.0], [-sa, 0.0, ca]])
    b = np.radians(plongee)
    cb, sb = np.cos(b), np.sin(b)
    Rx = np.array([[1.0, 0.0, 0.0], [0.0, cb, -sb], [0.0, sb, cb]])
    P = (V @ R.T) @ Rx.T

    marge = 0.90
    k = taille * marge / echelle
    sx = (P[:, 0] - centre[0]) * k + taille * 0.5
    sy = taille * 0.5 - (P[:, 1] - centre[1]) * k

    img = np.zeros((taille, taille), dtype=bool)
    ax, ay = sx[F[:, 0]], sy[F[:, 0]]
    bx, by = sx[F[:, 1]], sy[F[:, 1]]
    cx, cy = sx[F[:, 2]], sy[F[:, 2]]
    for i in range(F.shape[0]):
        x0 = int(max(0, np.floor(min(ax[i], bx[i], cx[i]))))
        x1 = int(min(taille - 1, np.ceil(max(ax[i], bx[i], cx[i]))))
        y0 = int(max(0, np.floor(min(ay[i], by[i], cy[i]))))
        y1 = int(min(taille - 1, np.ceil(max(ay[i], by[i], cy[i]))))
        if x1 < x0 or y1 < y0:
            continue
        xs = np.arange(x0, x1 + 1) + 0.5
        ys = np.arange(y0, y1 + 1) + 0.5
        gx, gy = np.meshgrid(xs, ys)
        d = (by[i] - cy[i]) * (ax[i] - cx[i]) + (cx[i] - bx[i]) * (ay[i] - cy[i])
        if abs(d) < 1e-12:
            continue
        w0 = ((by[i] - cy[i]) * (gx - cx[i]) + (cx[i] - bx[i]) * (gy - cy[i])) / d
        w1 = ((cy[i] - ay[i]) * (gx - cx[i]) + (ax[i] - cx[i]) * (gy - cy[i])) / d
        w2 = 1.0 - w0 - w1
        dans = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if dans.any():
            img[y0:y1 + 1, x0:x1 + 1] |= dans
    return img, k


def perimetre(masque_bool):
    """Pixels du masque ayant au moins un voisin hors masque : le contour."""
    import numpy as np
    m = masque_bool
    p = np.zeros_like(m)
    p[1:, :] |= m[1:, :] & ~m[:-1, :]
    p[:-1, :] |= m[:-1, :] & ~m[1:, :]
    p[:, 1:] |= m[:, 1:] & ~m[:, :-1]
    p[:, :-1] |= m[:, :-1] & ~m[:, 1:]
    return int(p.sum())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("reference")
    ap.add_argument("allege")
    ap.add_argument("--delta", type=float, required=True,
                    help="ecart de forme MAXIMAL en unites monde, mesure par le temoin C++")
    ap.add_argument("--vues", type=int, default=6)
    ap.add_argument("--taille", type=int, default=512)
    ap.add_argument("--images", default="")
    a = ap.parse_args()

    import numpy as np
    Vr, Fr = charger(a.reference)
    Va, Fa = charger(a.allege)

    # Le repere COMMUN vient de la reference, jamais de chacun pour soi.
    mn, mx = Vr.min(axis=0), Vr.max(axis=0)
    ctr = 0.5 * (mn + mx)
    ech = float(max(mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]))
    if ech <= 0.0:
        _refus("la reference a une boite englobante nulle")

    print("SILHOUETTE : %s  ->  %s" % (os.path.basename(a.reference), os.path.basename(a.allege)))
    print("  reference : %d sommets, %d faces | allege : %d sommets, %d faces"
          % (len(Vr), len(Fr), len(Va), len(Fa)))
    print("  delta monde = %.6f | echelle commune = %.6f" % (a.delta, ech))

    rouge = 0
    pires = []
    for i in range(a.vues):
        ang = 360.0 * i / a.vues
        plongee = 22.0
        Mr, k = masque(Vr, Fr, a.taille, ang, plongee, ctr, ech)

        # ── LE ZERO D'ABORD, A CHAQUE VUE ────────────────────────────────────
        # La reference contre ELLE-MEME. Si ce n'est pas 1,000000, l'instrument
        # est hors service et il le DIT au lieu de mesurer.
        Mr2, _ = masque(Vr, Fr, a.taille, ang, plongee, ctr, ech)
        if not np.array_equal(Mr, Mr2):
            _refus("le zero a echoue a la vue %d : deux rendus de la MEME reference different"
                   " -- l'instrument n'est pas deterministe" % i, 3)

        Ma, _ = masque(Va, Fa, a.taille, ang, plongee, ctr, ech)
        inter = int((Mr & Ma).sum())
        union = int((Mr | Ma).sum())
        if union == 0:
            _refus("vue %d : les deux silhouettes sont VIDES -- un IoU sur deux images vides"
                   " vaut 1 et ne prouve rien" % i, 3)
        iou = inter / float(union)
        aireRef = int(Mr.sum())
        periRef = perimetre(Mr)
        dpx = a.delta * k
        borne = (periRef * dpx / aireRef) if aireRef > 0 else 1.0

        etat = "VERT " if (1.0 - iou) <= borne else "ROUGE"
        if etat == "ROUGE":
            rouge += 1
        pires.append(1.0 - iou)
        print("  [%s] vue %3.0f deg : IoU=%.6f  (1-IoU=%.6f)  borne = P %d x delta %.3f px / A %d = %.6f"
              % (etat, ang, iou, 1.0 - iou, periRef, dpx, aireRef, borne))

        if a.images:
            try:
                os.makedirs(a.images, exist_ok=True)
                from PIL import Image
                rgb = np.zeros((a.taille, a.taille, 3), dtype=np.uint8)
                rgb[..., 0] = np.where(Mr & ~Ma, 230, 26)   # perdu  = rouge
                rgb[..., 1] = np.where(Ma & ~Mr, 200, 26)   # gagne  = vert
                rgb[..., 2] = 30
                comm = Mr & Ma
                rgb[comm] = (200, 200, 205)
                Image.fromarray(rgb).save(os.path.join(a.images, "silhouette_%03.0f.png" % ang))
            except Exception as e:
                sys.stderr.write("(images non ecrites : %s)\n" % e)

    print("  PIRE 1-IoU sur %d vues = %.6f" % (a.vues, max(pires)))
    print("VERDICT : %s (%d vue(s) rouge(s))" % ("VERT" if rouge == 0 else "ROUGE", rouge))
    return 0 if rouge == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
