# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/decouper_planche.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# LA PLANCHE MULTI-VUES (Q13), ET POURQUOI ELLE COMPTE MAINTENANT.
#
# Le 22/09 : la photo de Rodolf etait une PLANCHE DE DESSIN TECHNIQUE, TripoSR
# reconstruisait chaque vue comme un objet separe, et trois morceaux gris
# s'alignaient dans la scene.
# Le 25/09 : l'ajustement par projection REFUSE la porte, parce qu'une seule vue
# 3/4 ne determine pas les parametres d'un objet presque plan. Une planche EST la
# seconde vue qui manque -- et elle est gratuite, elle est deja dans l'image.
#
# CE QU'IL FAIT, ET RIEN D'AUTRE :
#   1. separe le sujet du fond (fond uni : c'est ce que sont les planches) ;
#   2. etiquette les composantes connexes ;
#   3. decide s'il s'agit d'une PLANCHE : plusieurs composantes de taille
#      COMPARABLE -- un objet avec un petit accessoire a cote n'en est pas une ;
#   4. decoupe, nomme par position (gauche a droite, haut en bas), ecrit.
#
# ⚠️ IL NE DEVINE PAS L'ANGLE DE CHAQUE VUE. Nommer « face », « profil »,
#    « dessus » demanderait de reconnaitre l'objet, ce qui est le probleme qu'on
#    essaie de resoudre. Les vues sont donc nommees par leur POSITION, et c'est
#    a l'ajustement de chercher les angles -- il sait deja le faire, il cherchait
#    un decalage commun sur trois vues.
#
# ⚠️ ET IL DOIT REFUSER DE COUPER CE QUI N'EST PAS UNE PLANCHE. Le critere est
#    mesure sur les deux cas : une planche fabriquee de N vues connues doit en
#    rendre N ; une image d'objet seul doit en rendre UNE. Un decoupeur qui
#    coupe toujours transformerait chaque objet en morceaux.
#
# USAGE : python decouper_planche.py <image> [--sortie DOSSIER] [--epreuve]
# -----------------------------------------------------------------------------
import argparse
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

for _f in (sys.stdout, sys.stderr):
    try:
        _f.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# Une composante sous ce pourcentage de la plus grande n'est pas une VUE : c'est
# un accessoire, une cote, une legende. Le chiffre est mesure plus bas (--epreuve).
PART_MIN = 0.15
# Une composante minuscule n'est meme pas un accessoire : c'est du bruit.
AIRE_MIN = 0.0008


def sujet(im):
    """Le masque du sujet. Le fond d'une planche est UNI : on le prend sur les
    quatre coins plutot que de supposer qu'il est blanc -- un fond gris clair ou
    bleute est courant, et supposer le blanc ferait rendre une seule composante
    qui couvre tout."""
    a = np.asarray(im.convert("RGB")).astype(int)
    h, w, _ = a.shape
    c = np.stack([a[0, 0], a[0, w - 1], a[h - 1, 0], a[h - 1, w - 1]])
    fond = c.mean(axis=0)
    d = np.abs(a - fond).sum(axis=2)
    return d > 40


def composantes(m):
    """Etiquetage 4-connexe, sans scipy (le venv ne l'a pas garanti)."""
    h, w = m.shape
    lab = np.zeros((h, w), dtype=np.int32)
    n = 0
    pile = []
    for y0 in range(h):
        for x0 in range(w):
            if not m[y0, x0] or lab[y0, x0]:
                continue
            n += 1
            pile.append((y0, x0))
            lab[y0, x0] = n
            while pile:
                y, x = pile.pop()
                for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    v, u = y + dy, x + dx
                    if 0 <= v < h and 0 <= u < w and m[v, u] and not lab[v, u]:
                        lab[v, u] = n
                        pile.append((v, u))
    return lab, n


def analyser(chemin):
    im = Image.open(chemin)
    m = sujet(im)
    lab, n = composantes(m)
    boites = []
    total = float(m.size)
    for k in range(1, n + 1):
        ys, xs = np.nonzero(lab == k)
        aire = len(ys) / total
        if aire < AIRE_MIN:
            continue
        boites.append(dict(k=k, aire=aire, y0=int(ys.min()), y1=int(ys.max()) + 1,
                           x0=int(xs.min()), x1=int(xs.max()) + 1))
    if not boites:
        return im, [], 0.0
    amax = max(b["aire"] for b in boites)
    gardees = [b for b in boites if b["aire"] >= PART_MIN * amax]
    # tri par position : haut en bas, puis gauche a droite
    gardees.sort(key=lambda b: (b["y0"] // max(1, im.size[1] // 4), b["x0"]))
    return im, gardees, amax


def main():
    p = argparse.ArgumentParser()
    p.add_argument("image", nargs="?")
    p.add_argument("--sortie", default="logs_genia3d/crea/q13")
    p.add_argument("--epreuve", action="store_true")
    a = p.parse_args()

    if a.epreuve:
        return epreuve(a.sortie)

    if not a.image:
        p.print_help()
        sys.exit(2)
    im, vues, amax = analyser(a.image)
    os.makedirs(a.sortie, exist_ok=True)
    stem = os.path.splitext(os.path.basename(a.image))[0]
    print("%s : %d vue(s) retenue(s)" % (a.image, len(vues)))
    if len(vues) <= 1:
        print("  CE N'EST PAS UNE PLANCHE : un seul sujet, rien a decouper.")
        sys.exit(0)
    for i, b in enumerate(vues):
        # une marge, parce qu'une silhouette collee au bord se juge mal
        mx = max(4, (b["x1"] - b["x0"]) // 20)
        my = max(4, (b["y1"] - b["y0"]) // 20)
        c = im.crop((max(0, b["x0"] - mx), max(0, b["y0"] - my),
                     min(im.size[0], b["x1"] + mx), min(im.size[1], b["y1"] + my)))
        out = os.path.join(a.sortie, "%s_vue%d.png" % (stem, i))
        c.save(out)
        print("  vue %d : %4d x %4d px, aire %.4f -> %s"
              % (i, b["x1"] - b["x0"], b["y1"] - b["y0"], b["aire"], out))


def epreuve(sortie):
    """L'EPREUVE, ECRITE AVANT LE CRITERE : on FABRIQUE des planches dont on
    connait le nombre de vues, et on verifie que le decoupeur les retrouve -- ET
    qu'il refuse de couper une image d'objet seul."""
    import numpy as _np
    corpus = "logs_genia3d/corpus_vues"
    os.makedirs(sortie, exist_ok=True)
    ids = ["00018_chair_B_wood", "00335_door-white", "00342_table_medium_long"]
    rouges = 0
    for ident in ids:
        z = _np.load(os.path.join(corpus, ident + ".npz"))
        for nv in (1, 2, 3, 4):
            # nv vues prises a des azimuts espaces, posees cote a cote sur blanc
            idx = [(k * (16 // max(nv, 1))) * 3 + 1 for k in range(nv)]
            tuiles = []
            for i in idx:
                r = z["rgba"][i].astype(float)
                al = r[..., 3:4] / 255.0
                tuiles.append((r[..., :3] * al + 255.0 * (1 - al)).astype(np.uint8))
            H = tuiles[0].shape[0]
            marge = 40
            L = sum(t.shape[1] for t in tuiles) + marge * (nv + 1)
            pl = np.ones((H + 2 * marge, L, 3), dtype=np.uint8) * 255
            x = marge
            for t in tuiles:
                pl[marge:marge + H, x:x + t.shape[1]] = t
                x += t.shape[1] + marge
            ch = os.path.join(sortie, "planche_%s_%d.png" % (ident[:5], nv))
            Image.fromarray(pl).save(ch)
            _, vues, _ = analyser(ch)
            ok = (len(vues) == nv)
            rouges += 0 if ok else 1
            print("%-22s %d vue(s) attendue(s) -> %d trouvee(s)  %s"
                  % (os.path.basename(ch), nv, len(vues), "vert" if ok else "ROUGE"))
    print("\n%d ROUGE(S)" % rouges)
    sys.exit(1 if rouges else 0)


if __name__ == "__main__":
    main()
